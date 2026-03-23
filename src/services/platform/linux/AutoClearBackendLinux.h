#pragma once

#include "../../interfaces/IAutoClearBackend.h"

// Uses a systemd user service unit that fires once at login then removes itself.
// If no systemd --user manager is active in the current session, the unit file is
// still written and can be picked up by a later login session.

class AutoClearBackendLinux : public IAutoClearBackend
{
    Q_OBJECT
public:
    explicit AutoClearBackendLinux(QObject* parent = nullptr);

    bool schedule() override;
    bool cancel()   override;
    bool exists()   override;

    QString lastError() const override { return m_lastError; }

private:
    bool hasSystemdUser() const;
    bool runProcess(const QString& program, const QStringList& args);

    QString m_lastError;

    static constexpr char kServiceName[] = "shutdown-timer-autoclear";
};
