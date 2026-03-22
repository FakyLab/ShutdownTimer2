#include "ShutdownBackendMacOS.h"

#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <QCoreApplication>
#include <unistd.h>

ShutdownBackendMacOS::ShutdownBackendMacOS(QObject* parent)
    : IShutdownBackend(parent)
{}

// -- Capability detection -----------------------------------------------------

bool ShutdownBackendMacOS::isHibernateAvailable()
{
    QProcess proc;
    proc.start("pmset", QStringList{"-g", "get", "hibernatemode"});
    proc.waitForFinished(3000);
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    return output.contains(QRegularExpression("hibernatemode\\s+[1-9]"));
}

bool ShutdownBackendMacOS::isSleepAvailable()
{
    return true;
}

// -- Helpers ------------------------------------------------------------------

QString ShutdownBackendMacOS::timerPlistPath() const
{
    return QDir::homePath() +
           "/Library/LaunchAgents/com.fakylab.shutdowntimer.timer.plist";
}

bool ShutdownBackendMacOS::runProcess(const QString& program, const QStringList& args)
{
    QProcess proc;
    proc.start(program, args);
    proc.waitForFinished(10000);
    if (proc.exitCode() != 0) {
        m_lastError = QString("%1 failed: %2")
                      .arg(program,
                           QString::fromUtf8(proc.readAllStandardError()).trimmed());
        emit errorOccurred(m_lastError);
        return false;
    }
    return true;
}

bool ShutdownBackendMacOS::runElevated(const QString& shellCmd)
{
    // Write AppleScript to a temp file — avoids inline escaping issues.
    // shellCmd must contain only double-quote-safe chars (fixed system commands).
    QString escaped = shellCmd;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");

    QTemporaryFile tmpScript(QDir::tempPath() + "/shutdowntimer_XXXXXX.applescript");
    tmpScript.setAutoRemove(false);
    if (!tmpScript.open()) {
        m_lastError = QString("Cannot create temp script: %1").arg(tmpScript.errorString());
        emit errorOccurred(m_lastError);
        return false;
    }
    QString scriptPath = tmpScript.fileName();
    {
        QTextStream out(&tmpScript);
        out << "do shell script \"" << escaped << "\" with administrator privileges\n";
        tmpScript.close();
    }

    QProcess proc;
    proc.start("osascript", QStringList{scriptPath});
    proc.waitForFinished(30000);
    QFile::remove(scriptPath);

    if (proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        m_lastError = err.isEmpty() ? "Authentication failed or was cancelled." : err;
        emit errorOccurred(m_lastError);
        return false;
    }
    return true;
}

// -- LaunchAgent scheduling ---------------------------------------------------
//
// For any scheduled shutdown/restart with seconds > 0, we register a
// LaunchAgent with StartCalendarInterval so the OS fires it at the right
// wall-clock time even if the app is closed.
//
// The agent runs: ShutdownTimer --execute-shutdown --action shutdown|restart --force 0|1
// The headless handler in main.cpp performs the actual shutdown.
//
// This mirrors exactly how AutoClearBackendMacOS works for the message clear.

bool ShutdownBackendMacOS::scheduleViaLaunchAgent(ShutdownAction action,
                                                   int seconds,
                                                   bool force)
{
    QDateTime fireTime = QDateTime::currentDateTime().addSecs(seconds);
    QString actionStr  = (action == ShutdownAction::Shutdown) ? "shutdown" : "restart";
    QString forceStr   = force ? "1" : "0";
    QString exePath    = QCoreApplication::applicationFilePath();

    // For AppImage this would be wrong, but we're macOS-only here.
    // For a DMG/app bundle, applicationFilePath() is the binary inside the .app.

    QString path = timerPlistPath();
    QDir().mkpath(QFileInfo(path).path());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = QString("Cannot write timer LaunchAgent: %1").arg(file.errorString());
        emit errorOccurred(m_lastError);
        return false;
    }

    QTextStream out(&file);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\"\n";
    out << "  \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    out << "<plist version=\"1.0\">\n";
    out << "<dict>\n";
    out << "    <key>Label</key>\n";
    out << "    <string>" << kTimerAgentLabel << "</string>\n";
    out << "    <key>ProgramArguments</key>\n";
    out << "    <array>\n";
    out << "        <string>" << exePath << "</string>\n";
    out << "        <string>--execute-shutdown</string>\n";
    out << "        <string>--action</string>\n";
    out << "        <string>" << actionStr << "</string>\n";
    out << "        <string>--force</string>\n";
    out << "        <string>" << forceStr << "</string>\n";
    out << "    </array>\n";
    out << "    <key>StartCalendarInterval</key>\n";
    out << "    <dict>\n";
    out << "        <key>Hour</key>   <integer>" << fireTime.time().hour()   << "</integer>\n";
    out << "        <key>Minute</key> <integer>" << fireTime.time().minute() << "</integer>\n";
    out << "    </dict>\n";
    // LaunchOnlyOnce: the handler removes the plist after running,
    // so this prevents repeated daily firings if removal somehow fails.
    out << "    <key>LaunchOnlyOnce</key>\n";
    out << "    <true/>\n";
    out << "</dict>\n";
    out << "</plist>\n";
    file.close();

    // Register the agent with launchd for the current user session.
    QString uid = QString::number(getuid());
    if (!runProcess("launchctl", QStringList{
            "bootstrap", QString("gui/%1").arg(uid), path})) {
        // bootstrap failed — clean up the plist we just wrote
        QFile::remove(path);
        return false;
    }

    return true;
}

bool ShutdownBackendMacOS::cancelLaunchAgent()
{
    QString path = timerPlistPath();
    if (!QFile::exists(path))
        return true;  // nothing to cancel

    QString uid = QString::number(getuid());
    runProcess("launchctl", QStringList{
        "bootout", QString("gui/%1").arg(uid), path});
    QFile::remove(path);
    return true;
}

// -- Main scheduling logic ----------------------------------------------------

bool ShutdownBackendMacOS::scheduleShutdown(ShutdownAction action,
                                             int seconds,
                                             bool force)
{
    // --- Sleep / Hibernate ---
    // These are immediate operations — the countdown just controls when
    // scheduleShutdown() is called, not when sleep happens.
    // Try pmset directly (works without root on most configs).
    // Fall back to osascript elevation on Sequoia where pmset needs root.
    if (action == ShutdownAction::Sleep || action == ShutdownAction::Hibernate) {
        m_pending = false;
        if (runProcess("pmset", QStringList{"sleepnow"}))
            return true;
        return runElevated("pmset sleepnow");
    }

    // --- Shutdown / Restart: immediate ---
    if (seconds <= 0) {
        m_pending = true;
        if (force) {
            // Force: skip save dialogs — requires root
            QString cmd = (action == ShutdownAction::Shutdown)
                          ? "shutdown -h now"
                          : "shutdown -r now";
            return runElevated(cmd);
        } else {
            // Graceful: trigger via System Events — no root needed,
            // same as user choosing Shut Down / Restart from the Apple menu.
            // Open apps will show their "save changes?" dialogs.
            QString event = (action == ShutdownAction::Shutdown)
                            ? "shut down"
                            : "restart";
            return runProcess("osascript",
                QStringList{"-e",
                    QString("tell application \"System Events\" to %1").arg(event)});
        }
    }

    // --- Shutdown / Restart: scheduled ---
    // Register a LaunchAgent so the timer fires even if the app is closed.
    // The agent runs our --execute-shutdown headless handler at the target time.
    // Password prompt (if needed for force mode) happens at that time.
    // For graceful mode no password is needed — System Events handles it.
    m_pending = true;
    return scheduleViaLaunchAgent(action, seconds, force);
}

bool ShutdownBackendMacOS::cancelShutdown()
{
    // Kill any in-process delayed shutdown
    if (m_delayedProcess) {
        m_delayedProcess->kill();
        m_delayedProcess->deleteLater();
        m_delayedProcess = nullptr;
    }

    // Remove the LaunchAgent if one was registered
    cancelLaunchAgent();

    // Cancel any native shutdown scheduled via `shutdown +N`
    // (belt-and-suspenders — runElevated is tolerant of no scheduled shutdown)
    runElevated("shutdown -c 2>/dev/null || true");

    m_pending = false;
    return true;
}
