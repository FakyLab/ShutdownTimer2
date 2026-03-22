#pragma once

// SlickNotification — post-login desktop notification for Linux Mint systems.
//
// slick-greeter (used by Linux Mint) has no text banner support on the
// graphical login screen. To compensate, we schedule a one-shot desktop
// notification that fires after the user logs in, using the standard XDG
// autostart mechanism.
//
// Two user-owned files (no root required):
//
//   ~/.config/shutdowntimer/pending-notification.json
//       Stores the message title and body as JSON.
//       Written by write(), deleted by clear() and --show-notification handler.
//
//   ~/.config/autostart/shutdowntimer-notify.desktop
//       XDG autostart entry. Fires ShutdownTimer --show-notification once
//       the desktop session is running. Deleted by the handler after firing,
//       and by clear() if the user removes the message before next login.
//
// These helpers are shared between MessageBackendLinux (AppImage/direct path)
// and MessageBackendLinuxDBus (.deb/AUR path via the helper service).

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>

#include "../../interfaces/IMessageBackend.h"  // for StartupMessage

namespace SlickNotification {

inline QString dataFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + "/pending-notification.json";
}

inline QString autostartDesktopPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + "/autostart/shutdowntimer-notify.desktop";
}

// Write the pending notification file and XDG autostart entry.
// Called after a successful message write on LightDMSlick systems.
// No root required — both files are in the user's config directory.
inline bool write(const StartupMessage& msg)
{
    // Write pending-notification.json
    QString dataPath = dataFilePath();
    QDir().mkpath(QFileInfo(dataPath).path());

    QJsonObject obj;
    obj["title"] = msg.title;
    obj["body"]  = msg.body;

    QFile f(dataPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    f.close();

    // Write XDG autostart .desktop file
    QString desktopPath = autostartDesktopPath();
    QDir().mkpath(QFileInfo(desktopPath).path());

    // applicationFilePath() returns the correct binary path for both
    // system installs (/usr/bin/ShutdownTimer) and AppImage runs.
    QString exePath = QCoreApplication::applicationFilePath();

    QFile desktop(desktopPath);
    if (!desktop.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream out(&desktop);
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Name=Shutdown Timer Notification\n";
    out << "Comment=Show pending Shutdown Timer login message\n";
    out << "Exec=" << exePath << " --show-notification\n";
    out << "Icon=shutdowntimer\n";
    out << "Hidden=false\n";
    out << "NoDisplay=true\n";
    out << "X-GNOME-Autostart-enabled=true\n";
    desktop.close();

    return true;
}

// Remove the pending notification file and XDG autostart entry.
// Called by clear() and by the --auto-clear handler.
inline void clear()
{
    QFile::remove(dataFilePath());
    QFile::remove(autostartDesktopPath());
}

} // namespace SlickNotification
