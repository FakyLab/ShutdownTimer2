#pragma once

#include <QObject>
#include <QDateTime>

#include "../models/TimerModel.h"
#include "../core/TimerEngine.h"
#include "../services/interfaces/IShutdownBackend.h"

class TimerController : public QObject
{
    Q_OBJECT
public:
    explicit TimerController(TimerModel*       model,
                             IShutdownBackend* shutdown,
                             bool              hibernateAvailable,
                             bool              sleepAvailable,
                             QObject*          parent = nullptr);

    const TimerModel* model() const { return m_model; }

    // Returns pre-cached values - no system calls, no blocking.
    bool isHibernateAvailable() const { return m_hibernateAvailable; }
    bool isSleepAvailable()     const { return m_sleepAvailable; }

public slots:
    void onStartCountdown(int totalSeconds, ShutdownAction action, bool force);
    void onStartScheduled(const QDateTime& target, ShutdownAction action, bool force);
    void onCancel();

signals:
    void countdownUpdated(int remainingSeconds);
    void timerStarted();
    void timerFinished();
    void timerCancelled();
    void statusMessage(const QString& msg);
    void errorOccurred(const QString& msg);
    void runningStateChanged(bool running);

private slots:
    void onEngineTick(int remaining);
    void onEngineTriggered();
    void onEngineInvalidTarget();

private:
    TimerModel*       m_model;
    TimerEngine*      m_engine;
    IShutdownBackend* m_shutdown;
    bool              m_hibernateAvailable;
    bool              m_sleepAvailable;
};
