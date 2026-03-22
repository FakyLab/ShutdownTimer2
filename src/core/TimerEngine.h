#pragma once

#include <QObject>
#include <QTimer>
#include <QDateTime>

class TimerEngine : public QObject
{
    Q_OBJECT
public:
    explicit TimerEngine(QObject* parent = nullptr);

    // Start countdown mode: fires after |seconds| from now
    void startCountdown(int totalSeconds);

    // Start scheduled mode: fires at |target| date/time
    void startScheduled(const QDateTime& target);

    // Stop/cancel the internal timer (does NOT cancel OS shutdown)
    void stop();

    bool      isRunning()        const { return m_timer.isActive(); }
    int       remainingSeconds() const { return m_remaining; }
    QDateTime targetTime()       const { return m_target; }

signals:
    // Emitted every second while running
    void tick(int remainingSeconds);

    // Emitted when countdown reaches zero
    void triggered();

    // Emitted if the target time is in the past when startScheduled() is called
    void invalidTarget();

private slots:
    void onTick();

private:
    QTimer    m_timer;
    int       m_remaining   = 0;
    int       m_lastEmitted = -1;
    QDateTime m_target;
};
