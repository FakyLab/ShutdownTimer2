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
    // Used only for force-mode shutdown/restart — the only case that genuinely
    // needs root. Must be called from a background thread; it blocks until
    // the user authenticates or cancels (up to 30 s).
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
// The headless handler in main.cpp performs the actual shutdown via
// sendSystemAppleEvent() — no TCC, no root for graceful mode.

bool ShutdownBackendMacOS::scheduleViaLaunchAgent(ShutdownAction action,
                                                   int seconds,
                                                   bool force)
{
    QDateTime fireTime = QDateTime::currentDateTime().addSecs(seconds);
    QString actionStr  = (action == ShutdownAction::Shutdown) ? "shutdown" : "restart";
    QString forceStr   = force ? "1" : "0";
    QString exePath    = QCoreApplication::applicationFilePath();

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
    // Include Hour, Minute, AND Second for full precision.
    // Without Second, launchd fires at the start of the target minute,
    // which can be up to 59 seconds early.
    out << "        <key>Hour</key>   <integer>" << fireTime.time().hour()   << "</integer>\n";
    out << "        <key>Minute</key> <integer>" << fireTime.time().minute() << "</integer>\n";
    out << "        <key>Second</key> <integer>" << fireTime.time().second() << "</integer>\n";
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
    // Always immediate — the countdown controls when this is called.
    // Use MACOS_AE_SLEEP via MacOSSystemEvents.mm (Foundation only, no Carbon).
    // No root required, no TCC, works on all macOS versions including Sequoia.
    if (action == ShutdownAction::Sleep || action == ShutdownAction::Hibernate) {
        m_pending = false;
        bool ok = sendSystemAppleEvent(MACOS_AE_SLEEP);
        if (!ok) {
            m_lastError = "Failed to send sleep event to system process.";
            emit errorOccurred(m_lastError);
        }
        return ok;
    }

    // --- Shutdown / Restart: immediate (T=0, timer fired) ---
    if (seconds <= 0) {
        m_pending = true;
        if (force) {
            // Force: skip save dialogs — genuinely requires root.
            QString cmd = (action == ShutdownAction::Shutdown)
                          ? "shutdown -h now"
                          : "shutdown -r now";
            return runElevated(cmd);
        } else {
            // Graceful: Core Event to system process via MacOSSystemEvents.mm.
            // No TCC prompt, no root, no osascript. Identical to the user
            // choosing Shut Down / Restart from the Apple menu.
            unsigned int eventID = (action == ShutdownAction::Shutdown)
                                   ? MACOS_AE_SHUTDOWN
                                   : MACOS_AE_RESTART;
            bool ok = sendSystemAppleEvent(eventID);
            if (!ok) {
                m_lastError = "Failed to send shutdown/restart event to system process.";
                emit errorOccurred(m_lastError);
            }
            return ok;
        }
    }

    // --- Shutdown / Restart: scheduled (seconds > 0) ---
    // Register a LaunchAgent so the timer fires even if the app is closed.
    m_pending = true;
    return scheduleViaLaunchAgent(action, seconds, force);
}

bool ShutdownBackendMacOS::cancelShutdown()
{
    // Remove the LaunchAgent if one was registered for a scheduled shutdown.
    cancelLaunchAgent();

    // Note: we do NOT call runElevated("shutdown -c") here.
    // Graceful mode uses sendSystemAppleEvent (fire-and-forget, cannot be
    // cancelled once sent). Force mode only calls `shutdown` at T=0 (by
    // which point cancellation is moot). Nothing to cancel at OS level.

    m_pending = false;
    return true;
}
