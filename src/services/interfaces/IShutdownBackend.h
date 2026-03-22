#pragma once

#include <QObject>
#include <QString>

enum class ShutdownAction {
    Shutdown,
    Restart,
    Hibernate,
    Sleep
};

class IShutdownBackend : public QObject
{
    Q_OBJECT
public:
    explicit IShutdownBackend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IShutdownBackend() = default;

    // Returns true if this backend can register a shutdown event with the OS
    // ahead of time (at Start) so the timer survives app closure.
    // When true, TimerController calls scheduleShutdown() at Start time with
    // the full remaining seconds in addition to the normal T=0 call.
    // Default: false — most backends execute immediately at T=0.
    virtual bool canScheduleAhead() const { return false; }

    // Execute the shutdown action. For Shutdown/Restart, |seconds| is the
    // OS-level delay (usually 0 - TimerEngine fires us at the right moment).
    // force=true closes apps without prompting them to save (Shutdown/Restart only).
    virtual bool scheduleShutdown(ShutdownAction action, int seconds, bool force) = 0;

    // Cancel a pending shutdown initiated by scheduleShutdown().
    virtual bool cancelShutdown() = 0;

    // Query hardware/OS support. Called once at startup to grey out UI options.
    virtual bool isHibernateAvailable() = 0;
    virtual bool isSleepAvailable()     = 0;

    virtual bool isPending() const = 0;

    virtual QString lastError() const = 0;

signals:
    void errorOccurred(const QString& message);
};
