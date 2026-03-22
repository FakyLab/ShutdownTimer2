#include "MessageBackendMacOS.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <unistd.h>

MessageBackendMacOS::MessageBackendMacOS(QObject* parent)
    : IMessageBackend(parent)
{}

// -- Path helpers -------------------------------------------------------------

QString MessageBackendMacOS::messageFilePath()
{
    // ~/Library/Application Support/ShutdownTimer/message.json
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/message.json";
}

QString MessageBackendMacOS::notifyPlistPath()
{
    return QDir::homePath()
           + "/Library/LaunchAgents/com.fakylab.shutdowntimer.notify.plist";
}

// -- IMessageBackend ----------------------------------------------------------

QString MessageBackendMacOS::platformDescription() const
{
    // Used in the UI label (see MessageView):
    //   "When saved, this message will be shown as a desktop notification on the %1."
    return tr("macOS desktop");
}

bool MessageBackendMacOS::runProcess(const QString& program, const QStringList& args)
{
    QProcess proc;
    proc.start(program, args);
    proc.waitForFinished(5000);
    if (proc.exitCode() != 0) {
        m_lastError = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return false;
    }
    return true;
}

bool MessageBackendMacOS::write(const StartupMessage& msg)
{
    // Write message.json
    QString filePath = messageFilePath();
    QDir().mkpath(QFileInfo(filePath).path());

    QJsonObject obj;
    obj["title"] = msg.title;
    obj["body"]  = msg.body;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write message file: %1").arg(f.errorString());
        return false;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    f.close();

    // Write and register the notification LaunchAgent
    if (!writeNotifyPlist()) {
        m_lastError = tr("Message saved but notification agent could not be registered.");
        // Non-fatal — message is saved, notification just won't auto-show
    }

    return true;
}

bool MessageBackendMacOS::read(StartupMessage& out)
{
    out.title.clear();
    out.body.clear();

    QFile f(messageFilePath());
    if (!f.exists())
        return true;  // no message set — not an error

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Cannot read message file: %1").arg(f.errorString());
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (doc.isNull() || !doc.isObject()) {
        m_lastError = tr("Message file is corrupted.");
        return false;
    }

    QJsonObject obj = doc.object();
    out.title = obj.value("title").toString().trimmed();
    out.body  = obj.value("body").toString().trimmed();
    return true;
}

bool MessageBackendMacOS::clear()
{
    // Remove message file
    QFile::remove(messageFilePath());

    // Bootout and remove notification LaunchAgent if it exists
    QString plist = notifyPlistPath();
    if (QFile::exists(plist)) {
        QString uid = QString::number(getuid());
        runProcess("launchctl",
            QStringList{"bootout", QString("gui/%1").arg(uid), plist});
        QFile::remove(plist);
    }

    return true;
}

// -- Private helpers ----------------------------------------------------------

bool MessageBackendMacOS::writeNotifyPlist()
{
    QString plistPath = notifyPlistPath();
    QDir().mkpath(QFileInfo(plistPath).path());

    QString exePath = QCoreApplication::applicationFilePath();

    QFile f(plistPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream out(&f);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\"\n";
    out << "  \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    out << "<plist version=\"1.0\">\n";
    out << "<dict>\n";
    out << "    <key>Label</key>\n";
    out << "    <string>" << kNotifyLabel << "</string>\n";
    out << "    <key>ProgramArguments</key>\n";
    out << "    <array>\n";
    out << "        <string>" << exePath << "</string>\n";
    out << "        <string>--show-notification</string>\n";
    out << "    </array>\n";
    // RunAtLoad=true fires the agent once when launchd loads it at next login.
    // We do NOT call launchctl bootstrap here — that would fire it immediately
    // in the current session. Instead we just write the plist and let launchd
    // pick it up automatically from ~/Library/LaunchAgents/ at the next login.
    out << "    <key>RunAtLoad</key>\n";
    out << "    <true/>\n";
    // LaunchOnlyOnce prevents re-firing after the process exits normally
    out << "    <key>LaunchOnlyOnce</key>\n";
    out << "    <true/>\n";
    out << "</dict>\n";
    out << "</plist>\n";
    f.close();

    // Do NOT call launchctl bootstrap — writing the plist to
    // ~/Library/LaunchAgents/ is sufficient. launchd loads all agents
    // in that directory automatically at the start of the next user session.
    // Calling bootstrap would fire --show-notification immediately NOW,
    // which would consume and delete the message before the user logs out.

    return true;
}
