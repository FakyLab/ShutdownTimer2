#include "MessageBackendMacOS.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

MessageBackendMacOS::MessageBackendMacOS(QObject* parent)
    : IMessageBackend(parent)
{}

// -- Path helpers -------------------------------------------------------------

QString MessageBackendMacOS::messageFilePath()
{
    // Hardcoded path — avoids QStandardPaths::AppDataLocation, which returns
    // different results depending on whether QCoreApplication has org/app name
    // set (GUI mode) or not (headless --show-notification mode).
    // GUI:      ~/Library/Application Support/ShutdownTimer/ShutdownTimer/message.json
    // Headless: ~/Library/Application Support/ShutdownTimer/message.json
    // Using a fixed path makes both modes agree on where the file lives.
    return QDir::homePath()
           + "/Library/Application Support/ShutdownTimer/message.json";
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

bool MessageBackendMacOS::write(const StartupMessage& msg)
{
    // Write message.json to the fixed path
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

    // Write the notification LaunchAgent plist.
    // This is just a file write — no launchctl bootstrap.
    // launchd scans ~/Library/LaunchAgents/ at the start of the next session
    // and loads any new plists it finds there.
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
    // Remove the message JSON file.
    QFile::remove(messageFilePath());

    // Remove the notification LaunchAgent plist file.
    // Do NOT call launchctl bootout here — if the notify agent is currently
    // running (firing at login right now), booting it out would kill it
    // mid-execution. Deleting the plist file is sufficient: launchd will not
    // re-load it at the next session since the file is gone.
    QFile::remove(notifyPlistPath());

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
    // RunAtLoad fires once when launchd loads this agent at the next login.
    // Do NOT call launchctl bootstrap now — that would fire --show-notification
    // immediately in the current session, consuming the message before logout.
    out << "    <key>RunAtLoad</key>\n";
    out << "    <true/>\n";
    // LaunchOnlyOnce: don't restart the process after it exits.
    out << "    <key>LaunchOnlyOnce</key>\n";
    out << "    <true/>\n";
    out << "</dict>\n";
    out << "</plist>\n";
    f.close();

    return true;
}
