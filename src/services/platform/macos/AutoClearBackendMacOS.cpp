#include "AutoClearBackendMacOS.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>
#include <unistd.h>

AutoClearBackendMacOS::AutoClearBackendMacOS(QObject* parent)
    : IAutoClearBackend(parent)
{}

QString AutoClearBackendMacOS::plistPath() const
{
    // Use the per-user LaunchAgents directory — no root required.
    // The --auto-clear handler will need root only to remove PolicyBanner files,
    // which it requests via osascript/sudo prompt at that point.
    QString home = QDir::homePath();
    return QString("%1/Library/LaunchAgents/%2.plist").arg(home, kAgentLabel);
}

bool AutoClearBackendMacOS::runProcess(const QString& program, const QStringList& args)
{
    QProcess proc;
    proc.start(program, args);
    proc.waitForFinished(5000);
    if (proc.exitCode() != 0) {
        m_lastError = QString("%1 failed: %2")
                      .arg(program,
                           QString::fromUtf8(proc.readAllStandardError()).trimmed());
        return false;
    }
    return true;
}

bool AutoClearBackendMacOS::schedule()
{
    QString exePath = QCoreApplication::applicationFilePath();
    QString path    = plistPath();

    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write LaunchAgent plist: %1").arg(file.errorString());
        return false;
    }

    QTextStream out(&file);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\"\n";
    out << "  \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    out << "<plist version=\"1.0\">\n";
    out << "<dict>\n";
    out << "    <key>Label</key>\n";
    out << "    <string>" << kAgentLabel << "</string>\n";
    out << "    <key>ProgramArguments</key>\n";
    out << "    <array>\n";
    out << "        <string>" << exePath << "</string>\n";
    out << "        <string>--auto-clear</string>\n";
    out << "    </array>\n";
    out << "    <key>RunAtLoad</key>\n";
    out << "    <true/>\n";
    // Only run once - the --auto-clear handler removes the plist itself
    out << "    <key>LaunchOnlyOnce</key>\n";
    out << "    <true/>\n";
    out << "</dict>\n";
    out << "</plist>\n";
    file.close();

    // Register with launchctl using modern bootstrap API (replaces deprecated `load`).
    // `launchctl bootstrap gui/<uid>` registers the agent for the current user session.
    QString uid = QString::number(getuid());
    runProcess("launchctl", QStringList{
        "bootstrap",
        QString("gui/%1").arg(uid),
        path
    });
    return true;
}

bool AutoClearBackendMacOS::cancel()
{
    QString path = plistPath();
    if (QFile::exists(path)) {
        QString uid = QString::number(getuid());
        // Bootout removes the agent from the current session
        runProcess("launchctl", QStringList{
            "bootout",
            QString("gui/%1").arg(uid),
            path
        });
        QFile::remove(path);
    }
    return true;
}

bool AutoClearBackendMacOS::exists()
{
    return QFile::exists(plistPath());
}
