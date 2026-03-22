#include "TimerController.h"

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

    m_engine->startCountdown(totalSeconds);

    emit timerStarted();
    emit runningStateChanged(true);
    emit statusMessage(tr("Timer started."));
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

    m_engine->startScheduled(target);

    emit timerStarted();
    emit runningStateChanged(true);
    emit statusMessage(tr("Timer started."));
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

    // Always call with seconds=0 - TimerEngine fires us exactly at t=0,
    // so the OS should execute immediately. The non-zero seconds path
    // is not used and has been intentionally removed.
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
