#pragma once

#include "../../interfaces/IMessageBackend.h"
#include <QString>

// macOS message backend — post-login desktop notification.
//
// No root required. The message is stored as a user-owned JSON file at a
// fixed path and delivered via a LaunchAgent that fires
// ShutdownTimer --show-notification at the next login.
// The notification is posted via UNUserNotificationCenter, attributed to
// "Shutdown Timer" — not to osascript.
//
// Storage:
//   ~/Library/Application Support/ShutdownTimer/message.json  (fixed path)
//   ~/Library/LaunchAgents/com.fakylab.shutdowntimer.notify.plist

class MessageBackendMacOS : public IMessageBackend
{
    Q_OBJECT
public:
    explicit MessageBackendMacOS(QObject* parent = nullptr);

    bool read(StartupMessage& out)        override;
    bool write(const StartupMessage& msg) override;
    bool clear()                          override;

    QString platformDescription() const   override;
    bool    isPostLogin() const           override { return true; }
    QString lastError() const             override { return m_lastError; }

    // Path helpers — shared with main.cpp --show-notification handler.
    // Both return fixed paths independent of QCoreApplication state,
    // so GUI and headless contexts always agree on the file locations.
    static QString messageFilePath();
    static QString notifyPlistPath();

private:
    bool writeNotifyPlist();

    QString m_lastError;

    static constexpr char kNotifyLabel[] = "com.fakylab.shutdowntimer.notify";
};
