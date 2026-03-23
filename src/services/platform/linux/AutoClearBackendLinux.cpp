#include "AutoClearBackendLinux.h"

#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

namespace {
QString quoteUnitArg(const QString& arg)
{
    QString escaped = arg;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return "\"" + escaped + "\"";
}
}

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
    QString exePath = qEnvironmentVariable("APPIMAGE");
    if (exePath.isEmpty())
        exePath = QCoreApplication::applicationFilePath();

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
    out << "Description=Shutdown Timer - auto-clear login message\n";
    out << "After=graphical-session.target\n\n";
    out << "[Service]\n";
    out << "Type=oneshot\n";
    out << "ExecStart=" << quoteUnitArg(exePath) << " --auto-clear\n";
    out << "ExecStartPost=rm -f " << quoteUnitArg(unitPath) << "\n\n";
    out << "[Install]\n";
    out << "WantedBy=default.target\n";
    unit.close();

    if (hasSystemdUser()) {
        if (!runProcess("systemctl", {"--user", "daemon-reload"})) {
            QFile::remove(unitPath);
            return false;
        }

        if (!runProcess("systemctl",
                        {"--user", "enable", QString(kServiceName) + ".service"})) {
            QFile::remove(unitPath);
            return false;
        }
    }

    return true;
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
