#include "TimerController.h"
#include <QDateTime>

TimerController::TimerController(TimerModel*       model,
                                 IShutdownBackend* shutdown,
                                 bool              hibernateAvailable,
                                 bool              sleepAvailable,
                                 QObject*          parent)
    : QObject(parent)
    , m_model(model)
    , m_engine(new TimerEngine(this))
    , m_shutdown(shutdown)
    , m_hibernateAvailable(hibernateAvailable)
    , m_sleepAvailable(sleepAvailable)
{
    connect(m_engine,   &TimerEngine::tick,
            this,       &TimerController::onEngineTick);
    connect(m_engine,   &TimerEngine::triggered,
            this,       &TimerController::onEngineTriggered);
    connect(m_engine,   &TimerEngine::invalidTarget,
            this,       &TimerController::onEngineInvalidTarget);
    connect(m_shutdown, &IShutdownBackend::errorOccurred,
            this,       &TimerController::errorOccurred);
}

void TimerController::onStartCountdown(int totalSeconds,
                                       ShutdownAction action,
                                       bool force)
{
    if (totalSeconds <= 0) {
        emit errorOccurred(tr("\nPlease set a time greater than zero."));
        return;
    }

    m_model->setAction(action);
    m_model->setForce(force);
    m_model->setMode(TimerMode::Countdown);
    m_model->setRunning(true);

    // On backends that can register with the OS ahead of time (macOS LaunchAgent),
    // schedule now so the timer survives app closure. The engine still counts down
    // for the UI — when it fires at T=0 it calls scheduleShutdown(0) again, which
    // on macOS hits the immediate path (LaunchAgent has already self-destructed).
    // Sleep/Hibernate return canScheduleAhead()=true but are immediate actions —
    // the LaunchAgent path ignores seconds=0 and only applies for seconds > 0.
    const bool supportsScheduleAhead =
        (action == ShutdownAction::Shutdown || action == ShutdownAction::Restart)
        && m_shutdown->canScheduleAhead();

    if (supportsScheduleAhead && totalSeconds > 0) {
        bool ok = m_shutdown->scheduleShutdown(action, totalSeconds, force);
        if (!ok) {
            m_model->setRunning(false);
            emit errorOccurred(m_shutdown->lastError());
            emit runningStateChanged(false);
            return;
        }
        emit statusMessage(tr("Timer registered with system. App can be closed."));
    } else {
        emit statusMessage(tr("Timer started."));
    }

    m_engine->startCountdown(totalSeconds);
    emit timerStarted();
    emit runningStateChanged(true);
}

void TimerController::onStartScheduled(const QDateTime& target,
                                       ShutdownAction   action,
                                       bool             force)
{
    m_model->setAction(action);
    m_model->setForce(force);
    m_model->setMode(TimerMode::Scheduled);
    m_model->setTargetTime(target);
    m_model->setRunning(true);

    int secondsUntil = static_cast<int>(QDateTime::currentDateTime().secsTo(target));
    const bool supportsScheduleAhead =
        (action == ShutdownAction::Shutdown || action == ShutdownAction::Restart)
        && m_shutdown->canScheduleAhead();

    if (supportsScheduleAhead && secondsUntil > 0) {
        bool ok = m_shutdown->scheduleShutdown(action, secondsUntil, force);
        if (!ok) {
            m_model->setRunning(false);
            emit errorOccurred(m_shutdown->lastError());
            emit runningStateChanged(false);
            return;
        }
        emit statusMessage(tr("Timer registered with system. App can be closed."));
    } else {
        emit statusMessage(tr("Timer started."));
    }

    m_engine->startScheduled(target);
    emit timerStarted();
    emit runningStateChanged(true);
}

void TimerController::onCancel()
{
    m_engine->stop();
    m_shutdown->cancelShutdown();
    m_model->setRunning(false);
    m_model->setRemaining(0);

    emit timerCancelled();
    emit runningStateChanged(false);
    emit statusMessage(tr("Cancelled."));
}

void TimerController::onEngineTick(int remaining)
{
    m_model->setRemaining(remaining);
    emit countdownUpdated(remaining);
}

void TimerController::onEngineTriggered()
{
    ShutdownAction action = m_model->action();
    bool force            = m_model->forceEnabled();

    // Always call with seconds=0 — TimerEngine fires at T=0, execute immediately.
    // On macOS (canScheduleAhead=true): a LaunchAgent was already registered at
    // Start time, so if the app is still open this is belt-and-suspenders.
    // cancelShutdown() on macOS removes the LaunchAgent before this fires,
    // so both paths are consistent.
    bool ok = m_shutdown->scheduleShutdown(action, 0, force);
    m_model->setRunning(false);

    if (ok) {
        QString msg;
        switch (action) {
            case ShutdownAction::Shutdown:  msg = tr("Shutting down...");  break;
            case ShutdownAction::Restart:   msg = tr("Restarting...");     break;
            case ShutdownAction::Hibernate: msg = tr("Hibernating...");    break;
            case ShutdownAction::Sleep:     msg = tr("Sleeping...");       break;
        }
        emit statusMessage(msg);
    }

    emit timerFinished();
    emit runningStateChanged(false);
}

void TimerController::onEngineInvalidTarget()
{
    m_model->setRunning(false);
    emit errorOccurred(
        tr("The scheduled time is in the past. Please choose a future time."));
    emit timerCancelled();
    emit runningStateChanged(false);
}
