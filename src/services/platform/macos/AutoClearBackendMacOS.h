#pragma once

#include "../../interfaces/IAutoClearBackend.h"

// macOS auto-clear backend — sentinel file approach.
//
// Instead of a separate LaunchAgent that races with the notify agent at login,
// we write a sentinel file alongside message.json. The --show-notification
// handler checks for this sentinel after posting the notification: if it
// exists, it deletes both the message and sentinel (auto-clear). If it does
// not exist, it leaves message.json in place (persistent mode).
//
// This eliminates the race condition where two LaunchAgents fire concurrently
// at login and both try to read/delete the same message.json.
//
// Sentinel path: ~/Library/Application Support/ShutdownTimer/autoclear

class AutoClearBackendMacOS : public IAutoClearBackend
{
    Q_OBJECT
public:
    explicit AutoClearBackendMacOS(QObject* parent = nullptr);

    bool schedule() override;  // writes sentinel file
    bool cancel()   override;  // removes sentinel file
    bool exists()   override;  // checks sentinel file

    QString lastError() const override { return m_lastError; }

    // Shared with main.cpp --show-notification handler
    static QString sentinelPath();

private:
    QString m_lastError;
};
