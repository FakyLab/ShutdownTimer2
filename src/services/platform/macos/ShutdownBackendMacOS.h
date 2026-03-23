#pragma once

#include "../../interfaces/IShutdownBackend.h"
#include "MacOSSystemEvents.h"
#include <QProcess>
#include <QString>

class ShutdownBackendMacOS : public IShutdownBackend
{
    Q_OBJECT
public:
    explicit ShutdownBackendMacOS(QObject* parent = nullptr);

    bool scheduleShutdown(ShutdownAction action, int seconds, bool force) override;
    bool cancelShutdown() override;

    bool isHibernateAvailable() override;
    bool isSleepAvailable()     override;

    // macOS can register a LaunchAgent ahead of time so the timer survives
    // app closure. Sleep/Hibernate are excluded - they are always immediate.
    bool canScheduleAhead() const override { return true; }

    bool isPending() const override { return m_pending; }
    QString lastError() const override { return m_lastError; }

    static constexpr const char* kTimerAgentLabel =
        "com.fakylab.shutdowntimer.timer";

    static bool sendSystemAppleEvent(unsigned int eventID)
    {
        return MacOSSendSystemAppleEvent(eventID) != 0;
    }

private:
    bool runElevated(const QString& shellCmd);
    bool runProcess(const QString& program,
                    const QStringList& args,
                    int timeoutMs = 10000,
                    bool emitFailure = true);
    bool validateLaunchAgentPlist(const QString& path);
    bool bootstrapLaunchAgent(const QString& path);
    bool scheduleViaLaunchAgent(ShutdownAction action, int seconds, bool force);
    bool cancelLaunchAgent();
    QString timerPlistPath() const;

    bool    m_pending   = false;
    QString m_lastError;
};
