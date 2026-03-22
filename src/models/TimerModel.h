#pragma once

#include <QObject>
#include <QDateTime>
#include "../services/interfaces/IShutdownBackend.h"

enum class TimerMode {
    Countdown,
    Scheduled
};

class TimerModel : public QObject
{
    Q_OBJECT
public:
    explicit TimerModel(QObject* parent = nullptr);

    // State getters
    TimerMode    mode()             const { return m_mode; }
    ShutdownAction action()         const { return m_action; }
    bool         forceEnabled()     const { return m_force; }
    int          remainingSeconds() const { return m_remaining; }
    bool         isRunning()        const { return m_running; }
    QDateTime    targetTime()       const { return m_target; }

    // State setters (called by controller)
    void setMode(TimerMode mode)          { m_mode = mode; }
    void setAction(ShutdownAction action) { m_action = action; }
    void setForce(bool force)             { m_force = force; }
    void setRemaining(int secs)           { m_remaining = secs; }
    void setRunning(bool running)         { m_running = running; }
    void setTargetTime(const QDateTime& t){ m_target = t; }

private:
    TimerMode      m_mode      = TimerMode::Countdown;
    ShutdownAction m_action    = ShutdownAction::Shutdown;
    bool           m_force     = false;
    int            m_remaining = 0;
    bool           m_running   = false;
    QDateTime      m_target;
};
