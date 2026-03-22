#pragma once

#include "../../interfaces/IAutoClearBackend.h"

// Uses a LaunchAgent plist in /Library/LaunchAgents/ that fires once at login.
// The --auto-clear handler deletes the plist and unloads the agent after running.

class AutoClearBackendMacOS : public IAutoClearBackend
{
    Q_OBJECT
public:
    explicit AutoClearBackendMacOS(QObject* parent = nullptr);

    bool schedule() override;
    bool cancel()   override;
    bool exists()   override;

    QString lastError() const override { return m_lastError; }

private:
    bool runProcess(const QString& program, const QStringList& args);
    QString plistPath() const;

    QString m_lastError;

    static constexpr char kAgentLabel[] = "com.fakylab.shutdowntimer.autoclear";
};
