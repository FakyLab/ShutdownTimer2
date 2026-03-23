#include "ShutdownBackendMacOS.h"

#include <QProcess>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTextStream>
#include <QDateTime>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QThread>
#include <unistd.h>

ShutdownBackendMacOS::ShutdownBackendMacOS(QObject* parent)
    : IShutdownBackend(parent)
{}

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

QString ShutdownBackendMacOS::timerPlistPath() const
{
    return QDir::homePath() +
           "/Library/LaunchAgents/com.fakylab.shutdowntimer.timer.plist";
}

bool ShutdownBackendMacOS::runProcess(const QString& program,
                                      const QStringList& args,
                                      int timeoutMs,
                                      bool emitFailure)
{
    QProcess proc;
    proc.start(program, args);

    if (!proc.waitForStarted(3000)) {
        m_lastError = QString("%1 failed to start: %2")
                      .arg(program, proc.errorString());
        if (emitFailure)
            emit errorOccurred(m_lastError);
        return false;
    }

    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(1000);
        m_lastError = QString("%1 timed out.").arg(program);
        if (emitFailure)
            emit errorOccurred(m_lastError);
        return false;
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        QString detail = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (detail.isEmpty())
            detail = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (detail.isEmpty())
            detail = QString("exit code %1").arg(proc.exitCode());
        m_lastError = QString("%1 failed: %2").arg(program, detail);
        if (emitFailure)
            emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool ShutdownBackendMacOS::runElevated(const QString& shellCmd)
{
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

    const QString scriptPath = tmpScript.fileName();
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

bool ShutdownBackendMacOS::validateLaunchAgentPlist(const QString& path)
{
    if (runProcess("plutil", {"-lint", path}, 10000, false))
        return true;

    m_lastError = QString("LaunchAgent plist validation failed: %1").arg(m_lastError);
    emit errorOccurred(m_lastError);
    return false;
}

bool ShutdownBackendMacOS::bootstrapLaunchAgent(const QString& path)
{
    const QString domain = QString("gui/%1").arg(QString::number(getuid()));

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (runProcess("launchctl", {"bootstrap", domain, path}, 10000, false))
            return true;

        const QString bootstrapError = m_lastError;
        if (attempt == 0) {
            runProcess("launchctl", {"bootout", domain, path}, 10000, false);
            QThread::msleep(150);
            continue;
        }

        m_lastError = QString("launchctl bootstrap failed: %1").arg(bootstrapError);
        emit errorOccurred(m_lastError);
        return false;
    }

    return false;
}

bool ShutdownBackendMacOS::scheduleViaLaunchAgent(ShutdownAction action,
                                                  int seconds,
                                                  bool force)
{
    const QDateTime fireTime = QDateTime::currentDateTime().addSecs(seconds);
    const QString actionStr  = (action == ShutdownAction::Shutdown) ? "shutdown" : "restart";
    const QString forceStr   = force ? "1" : "0";
    const QString exePath    = QCoreApplication::applicationFilePath();

    const QString path = timerPlistPath();
    QDir().mkpath(QFileInfo(path).path());

    cancelLaunchAgent();

    QSaveFile file(path);
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
    out << "        <key>Second</key> <integer>" << fireTime.time().second() << "</integer>\n";
    out << "    </dict>\n";
    out << "    <key>LaunchOnlyOnce</key>\n";
    out << "    <true/>\n";
    out << "</dict>\n";
    out << "</plist>\n";

    if (!file.commit()) {
        m_lastError = QString("Cannot commit timer LaunchAgent: %1").arg(file.errorString());
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!validateLaunchAgentPlist(path)) {
        QFile::remove(path);
        return false;
    }

    if (!bootstrapLaunchAgent(path)) {
        QFile::remove(path);
        return false;
    }

    return true;
}

bool ShutdownBackendMacOS::cancelLaunchAgent()
{
    const QString path = timerPlistPath();
    if (!QFile::exists(path))
        return true;

    const QString domain = QString("gui/%1").arg(QString::number(getuid()));
    runProcess("launchctl", {"bootout", domain, path}, 10000, false);
    QFile::remove(path);
    return true;
}

bool ShutdownBackendMacOS::scheduleShutdown(ShutdownAction action,
                                            int seconds,
                                            bool force)
{
    if (action == ShutdownAction::Sleep || action == ShutdownAction::Hibernate) {
        const bool ok = sendSystemAppleEvent(MACOS_AE_SLEEP);
        if (!ok) {
            m_lastError = "Failed to send sleep event to system process.";
            emit errorOccurred(m_lastError);
        }
        m_pending = false;
        return ok;
    }

    if (seconds <= 0) {
        bool ok = false;
        if (force) {
            const QString cmd = (action == ShutdownAction::Shutdown)
                                ? "shutdown -h now"
                                : "shutdown -r now";
            ok = runElevated(cmd);
        } else {
            const unsigned int eventID = (action == ShutdownAction::Shutdown)
                                         ? MACOS_AE_SHUTDOWN
                                         : MACOS_AE_RESTART;
            ok = sendSystemAppleEvent(eventID);
            if (!ok) {
                m_lastError = "Failed to send shutdown/restart event to system process.";
                emit errorOccurred(m_lastError);
            }
        }
        m_pending = ok;
        return ok;
    }

    const bool ok = scheduleViaLaunchAgent(action, seconds, force);
    m_pending = ok;
    return ok;
}

bool ShutdownBackendMacOS::cancelShutdown()
{
    cancelLaunchAgent();
    m_pending = false;
    return true;
}
