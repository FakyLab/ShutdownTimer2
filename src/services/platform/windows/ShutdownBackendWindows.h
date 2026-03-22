#pragma once

#include "../../interfaces/IShutdownBackend.h"
#include <windows.h>
#include <powrprof.h>

class ShutdownBackendWindows : public IShutdownBackend
{
    Q_OBJECT
public:
    explicit ShutdownBackendWindows(QObject* parent = nullptr);

    bool scheduleShutdown(ShutdownAction action, int seconds, bool force) override;
    bool cancelShutdown() override;

    bool isHibernateAvailable() override;
    bool isSleepAvailable()     override;

    bool isPending() const override { return m_pending; }
    QString lastError() const override { return m_lastError; }

private:
    bool acquireShutdownPrivilege();

    bool    m_pending   = false;
    QString m_lastError;
};
