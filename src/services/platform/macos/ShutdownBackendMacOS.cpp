#include <QRegularExpression>
#include "ShutdownBackendMacOS.h"

#include <QProcess>

ShutdownBackendMacOS::ShutdownBackendMacOS(QObject* parent)
    : IShutdownBackend(parent)
{}

bool ShutdownBackendMacOS::isHibernateAvailable()
{
    // macOS supports safe sleep (hibernate) on all modern Macs
    // Check pmset hibernatemode - mode 3 or 25 = hibernate-capable
    QProcess proc;
    proc.start("pmset", QStringList{"-g", "get", "hibernatemode"});
    proc.waitForFinished(3000);
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    // Any hibernatemode value > 0 means some form of hibernate is available
    return output.contains(QRegularExpression("hibernatemode\\s+[1-9]"));
}

bool ShutdownBackendMacOS::isSleepAvailable()
{
    // Sleep is always available on macOS
    return true;
}

bool ShutdownBackendMacOS::runOsascript(const QString& script)
{
    QProcess proc;
    proc.start("osascript", QStringList{"-e", script});
    proc.waitForFinished(10000);
    if (proc.exitCode() != 0) {
        m_lastError = QString("osascript failed: %1")
                      .arg(QString::fromUtf8(proc.readAllStandardError()).trimmed());
        emit errorOccurred(m_lastError);
        return false;
    }
    return true;
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

bool ShutdownBackendMacOS::scheduleShutdown(ShutdownAction action, int seconds, bool force)
{
    Q_UNUSED(force)

    if (action == ShutdownAction::Sleep) {
        m_pending = false;
        return runOsascript("tell application \"System Events\" to sleep");
    }

    if (action == ShutdownAction::Hibernate) {
        m_pending = false;
        // pmset sleepnow with hibernatemode triggers safe sleep
        return runProcess("pmset", QStringList{"sleepnow"});
    }

    // For Shutdown/Restart use `shutdown` with a delay in seconds via `at` or direct
    // macOS `shutdown` takes time as +N minutes or absolute HH:MM - convert seconds
    if (seconds <= 0) {
        if (action == ShutdownAction::Shutdown)
            return runProcess("shutdown", QStringList{"-h", "now"});
        else
            return runProcess("shutdown", QStringList{"-r", "now"});
    }

    // Use macOS native shutdown scheduling.
    // `shutdown` accepts +N minutes. Convert seconds to whole minutes,
    // rounding up so we don't shut down early.
    // For anything under 60 seconds, use a sleep+shutdown background process
    // and store it so cancel() can kill it reliably.
    if (seconds < 60) {
        // Sub-minute: background process with stored reference for cancel
        QString cmd = (action == ShutdownAction::Shutdown)
                      ? "shutdown -h now"
                      : "shutdown -r now";

        if (m_delayedProcess) {
            m_delayedProcess->kill();
            m_delayedProcess->deleteLater();
            m_delayedProcess = nullptr;
        }

        m_delayedProcess = new QProcess(this);
        connect(m_delayedProcess,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this]() {
                    m_delayedProcess = nullptr;
                    m_pending = false;
                });
        m_delayedProcess->start("bash", QStringList{
            "-c",
            QString("sleep %1 && %2").arg(seconds).arg(cmd)
        });
        m_pending = true;
        return true;
    }

    // >= 60 seconds: use native shutdown scheduling (rounds to minutes)
    int minutes = (seconds + 59) / 60;  // round up
    QString flag = (action == ShutdownAction::Shutdown) ? "-h" : "-r";
    m_pending = true;
    return runProcess("shutdown", QStringList{
        flag,
        QString("+%1").arg(minutes)
    });
}

bool ShutdownBackendMacOS::cancelShutdown()
{
    // Kill sub-minute background process if active
    if (m_delayedProcess) {
        m_delayedProcess->kill();
        m_delayedProcess->deleteLater();
        m_delayedProcess = nullptr;
    }

    // Cancel any native scheduled shutdown
    runProcess("shutdown", QStringList{"-c"});
    m_pending = false;
    return true;
}
