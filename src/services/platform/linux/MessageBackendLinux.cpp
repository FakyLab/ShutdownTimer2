#include "MessageBackendLinux.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString quoteExecArg(const QString& arg)
{
    QString escaped = arg;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("$", "\\$");
    escaped.replace("`", "\\`");
    return "\"" + escaped + "\"";
}
}

MessageBackendLinux::MessageBackendLinux(QObject* parent)
    : IMessageBackend(parent)
{}

// -- Path helpers -------------------------------------------------------------

QString MessageBackendLinux::messageFilePath()
{
    // Use GenericConfigLocation with a hardcoded subdirectory so the path is
    // always ~/.config/shutdowntimer/message.json regardless of whether
    // QCoreApplication has applicationName/organizationName set.
    //
    // AppConfigLocation would give different paths depending on whether
    // applicationName and organizationName are set (they differ between the
    // main app and headless handlers like --auto-clear and --show-notification).
    // GenericConfigLocation is always ~/.config/ - stable and context-independent.
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + "/shutdowntimer/message.json";
}

QString MessageBackendLinux::autostartDesktopPath()
{
    // ~/.config/autostart/shutdowntimer-notify.desktop
    // XDG Desktop Application Autostart Specification - fires on any
    // freedesktop-compliant DE (GNOME, KDE, XFCE, Cinnamon, MATE, etc.)
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + "/autostart/shutdowntimer-notify.desktop";
}

// -- IMessageBackend ----------------------------------------------------------

QString MessageBackendLinux::platformDescription() const
{
    // Used in the UI label (see MessageView):
    //   "When saved, this message will be shown as a desktop notification on the %1."
    return tr("Linux desktop");
}

bool MessageBackendLinux::write(const StartupMessage& msg)
{
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

    if (!writeAutostartEntry()) {
        QFile::remove(filePath);
        return false;
    }

    return true;
}

bool MessageBackendLinux::read(StartupMessage& out)
{
    out.title.clear();
    out.body.clear();

    QFile f(messageFilePath());
    if (!f.exists())
        return true;  // no message set - not an error

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

bool MessageBackendLinux::clear()
{
    QFile::remove(messageFilePath());
    QFile::remove(autostartDesktopPath());
    return true;
}

// -- Private helpers ----------------------------------------------------------

bool MessageBackendLinux::writeAutostartEntry()
{
    QString desktopPath = autostartDesktopPath();
    QDir().mkpath(QFileInfo(desktopPath).path());

    // For AppImage runs, applicationFilePath() returns the path inside the
    // squashfs mount (/tmp/.mount_xxx/usr/bin/ShutdownTimer) which is
    // session-specific and won't exist at the next login.
    // $APPIMAGE is set by the AppImage runtime to the actual .AppImage file
    // path which is stable across sessions - use it when available.
    QString exePath = qEnvironmentVariable("APPIMAGE");
    if (exePath.isEmpty())
        exePath = QCoreApplication::applicationFilePath();

    QFile f(desktopPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write autostart entry: %1").arg(f.errorString());
        return false;
    }

    QTextStream out(&f);
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Name=Shutdown Timer Notification\n";
    out << "Comment=Show pending Shutdown Timer message as a desktop notification\n";
    out << "Exec=" << quoteExecArg(exePath) << " --show-notification\n";
    out << "Icon=shutdowntimer\n";
    out << "Hidden=false\n";
    out << "NoDisplay=true\n";
    out << "X-GNOME-Autostart-enabled=true\n";
    out << "Terminal=false\n";
    f.close();

    return true;
}
