#pragma once

#include "../../interfaces/IMessageBackend.h"
#include <QString>

// MessageBackendLinux
//
// Stores the startup message as a JSON file in the user's config directory.
// No root access required — everything is user-owned.
//
// On write():
//   - Saves message to ~/.config/ShutdownTimer/message.json
//   - Writes ~/.config/autostart/shutdowntimer-notify.desktop
//     (XDG autostart entry that fires --show-notification at next login)
//
// On clear():
//   - Removes message.json and the autostart .desktop
//
// On read():
//   - Reads message.json
//
// At next login, the XDG autostart entry fires:
//   ShutdownTimer --show-notification
// which reads message.json, shows a desktop notification via notify-send,
// then removes both files.

class MessageBackendLinux : public IMessageBackend
{
    Q_OBJECT
public:
    explicit MessageBackendLinux(QObject* parent = nullptr);

    bool read(StartupMessage& out)        override;
    bool write(const StartupMessage& msg) override;
    bool clear()                          override;

    QString platformDescription() const   override;
    QString lastError() const             override { return m_lastError; }

    // Path helpers — shared with main.cpp handlers
    static QString messageFilePath();
    static QString autostartDesktopPath();

private:
    bool writeAutostartEntry();

    QString m_lastError;
};
