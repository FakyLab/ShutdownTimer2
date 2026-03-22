#pragma once

#include "../../interfaces/IShutdownBackend.h"
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
    // app closure. Sleep/Hibernate are excluded — they're always immediate.
    bool canScheduleAhead() const override { return true; }

    bool isPending() const override { return m_pending; }
    QString lastError() const override { return m_lastError; }

    // Label used for the LaunchAgent that fires the scheduled shutdown.
    // Different from the auto-clear agent label.
    static constexpr const char* kTimerAgentLabel =
        "com.fakylab.shutdowntimer.timer";

private:
    // Run a shell command as root via osascript (native password dialog).
    // shellCmd must use only double-quote-safe content — no user-supplied data.
    bool runElevated(const QString& shellCmd);

    // Run a command unprivileged. Returns true on exit code 0.
    bool runProcess(const QString& program, const QStringList& args);

    // Schedule a shutdown/restart via a LaunchAgent so it fires at the
    // target wall-clock time even if the app is closed.
    bool scheduleViaLaunchAgent(ShutdownAction action, int seconds, bool force);

    // Cancel any scheduled LaunchAgent timer.
    bool cancelLaunchAgent();

    // Path to the timer LaunchAgent plist in ~/Library/LaunchAgents/.
    QString timerPlistPath() const;

    // Stored reference for sub-minute background osascript process.
    QProcess* m_delayedProcess = nullptr;
    bool      m_pending        = false;
    QString   m_lastError;
};
