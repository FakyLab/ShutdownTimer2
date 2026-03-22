#pragma once

#include "../../interfaces/IShutdownBackend.h"

class ShutdownBackendLinux : public IShutdownBackend
{
    Q_OBJECT
public:
    explicit ShutdownBackendLinux(QObject* parent = nullptr);

    bool scheduleShutdown(ShutdownAction action, int seconds, bool force) override;
    bool cancelShutdown() override;

    bool isHibernateAvailable() override;
    bool isSleepAvailable()     override;

    bool isPending() const override { return m_pending; }
    QString lastError() const override { return m_lastError; }

private:
    bool runSystemctl(const QStringList& args);

    bool    m_pending   = false;
    QString m_lastError;
};
