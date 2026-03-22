#include "ShutdownBackendLinux.h"

#include <QProcess>

ShutdownBackendLinux::ShutdownBackendLinux(QObject* parent)
    : IShutdownBackend(parent)
{}

bool ShutdownBackendLinux::runSystemctl(const QStringList& args)
{
    QProcess proc;
    proc.start("systemctl", args);
    proc.waitForFinished(5000);

    if (proc.exitCode() != 0) {
        m_lastError = QString("systemctl %1 failed: %2")
                      .arg(args.join(' '),
                           QString::fromUtf8(proc.readAllStandardError()).trimmed());
        emit errorOccurred(m_lastError);
        return false;
    }
    return true;
}

bool ShutdownBackendLinux::isHibernateAvailable()
{
    QProcess proc;
    proc.start("systemctl", QStringList{"can-hibernate"});
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}

bool ShutdownBackendLinux::isSleepAvailable()
{
    QProcess proc;
    proc.start("systemctl", QStringList{"can-suspend"});
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}

bool ShutdownBackendLinux::scheduleShutdown(ShutdownAction action, int seconds, bool force)
{
    Q_UNUSED(seconds) // Always 0 — TimerEngine fires us at t=0, we execute immediately.
    Q_UNUSED(force)   // Linux `shutdown` has no equivalent of Windows force-close.

    switch (action) {
        case ShutdownAction::Hibernate:
            m_pending = false;
            return runSystemctl({"hibernate"});

        case ShutdownAction::Sleep:
            m_pending = false;
            return runSystemctl({"suspend"});

        case ShutdownAction::Shutdown:
        case ShutdownAction::Restart: {
            QStringList args = (action == ShutdownAction::Shutdown)
                               ? QStringList{"--poweroff", "now"}
                               : QStringList{"--reboot",   "now"};

            QProcess proc;
            proc.start("shutdown", args);
            proc.waitForFinished(5000);

            if (proc.exitCode() != 0) {
                m_lastError = QString("shutdown failed: %1")
                              .arg(QString::fromUtf8(
                                  proc.readAllStandardError()).trimmed());
                emit errorOccurred(m_lastError);
                return false;
            }

            m_pending = true;
            return true;
        }
    }
    return false;
}

bool ShutdownBackendLinux::cancelShutdown()
{
    QProcess proc;
    proc.start("shutdown", QStringList{"-c"});
    proc.waitForFinished(5000);
    m_pending = false;

    if (proc.exitCode() != 0) {
        m_lastError = QString("shutdown -c failed: %1")
                      .arg(QString::fromUtf8(proc.readAllStandardError()).trimmed());
        emit errorOccurred(m_lastError);
        return false;
    }
    return true;
}
