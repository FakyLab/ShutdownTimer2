#pragma once

#include "../../interfaces/IAutoClearBackend.h"

// Uses a systemd user service drop-in that fires once at login then removes itself.
// Falls back to a ~/.profile one-shot script if systemd user services are unavailable.

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
