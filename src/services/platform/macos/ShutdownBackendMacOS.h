#pragma once

#include "../../interfaces/IShutdownBackend.h"
#include <QProcess>

class ShutdownBackendMacOS : public IShutdownBackend
{
    Q_OBJECT
public:
    explicit ShutdownBackendMacOS(QObject* parent = nullptr);

    bool scheduleShutdown(ShutdownAction action, int seconds, bool force) override;
    bool cancelShutdown() override;

    bool isHibernateAvailable() override;
    bool isSleepAvailable()     override;

    bool isPending() const override { return m_pending; }
    QString lastError() const override { return m_lastError; }

private:
    bool runProcess(const QString& program, const QStringList& args);

    // Stored reference for sub-minute delayed shutdowns so cancel() can
    // kill the background bash process reliably.
    QProcess* m_delayedProcess = nullptr;
    bool      m_pending        = false;
    QString   m_lastError;
};
