#include "AutoClearBackendLinux.h"

#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

AutoClearBackendLinux::AutoClearBackendLinux(QObject* parent)
    : IAutoClearBackend(parent)
{}

bool AutoClearBackendLinux::hasSystemdUser() const
{
    QProcess proc;
    proc.start("systemctl", QStringList{"--user", "status"});
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}

bool AutoClearBackendLinux::runProcess(const QString& program, const QStringList& args)
{
    QProcess proc;
    proc.start(program, args);
    proc.waitForFinished(5000);
    if (proc.exitCode() != 0) {
        m_lastError = QString("%1 %2 failed: %3")
                      .arg(program, args.join(' '),
                           QString::fromUtf8(proc.readAllStandardError()).trimmed());
        return false;
    }
    return true;
}

bool AutoClearBackendLinux::schedule()
{
    QString exePath = QCoreApplication::applicationFilePath();

    // Write a systemd user service unit that runs once at login
    QString configHome = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    QString unitDir  = configHome + "/systemd/user/";
    QString unitPath = unitDir + QString(kServiceName) + ".service";

    QDir().mkpath(unitDir);
    QFile unit(unitPath);
    if (!unit.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write systemd unit: %1").arg(unit.errorString());
        return false;
    }

    QTextStream out(&unit);
    out << "[Unit]\n";
    out << "Description=Shutdown Timer — auto-clear login message\n";
    out << "After=graphical-session.target\n\n";
    out << "[Service]\n";
    out << "Type=oneshot\n";
    out << "ExecStart=" << exePath << " --auto-clear\n";
    // Remove the unit file after running so it doesn't fire again.
    // Use the bare 'rm' command (no hardcoded /bin/) for compatibility
    // with systems using merged /usr (where /bin is a symlink to /usr/bin).
    out << "ExecStartPost=rm -f " << unitPath << "\n\n";
    out << "[Install]\n";
    out << "WantedBy=default.target\n";
    unit.close();

    // Enable and start via systemd --user
    if (hasSystemdUser()) {
        runProcess("systemctl", {"--user", "daemon-reload"});
        return runProcess("systemctl",
                          {"--user", "enable", QString(kServiceName) + ".service"});
    }

    return true; // Unit file is in place; will be picked up at next login
}

bool AutoClearBackendLinux::cancel()
{
    QString configHome = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    QString unitPath = configHome + "/systemd/user/"
                       + QString(kServiceName) + ".service";

    if (hasSystemdUser())
        runProcess("systemctl", {"--user", "disable", QString(kServiceName) + ".service"});

    QFile::remove(unitPath);
    return true;
}

bool AutoClearBackendLinux::exists()
{
    QString configHome = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    QString unitPath = configHome + "/systemd/user/"
                       + QString(kServiceName) + ".service";
    return QFile::exists(unitPath);
}
