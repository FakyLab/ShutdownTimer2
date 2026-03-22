#pragma once

#include <QObject>
#include <QString>

class IAutoClearBackend : public QObject
{
    Q_OBJECT
public:
    explicit IAutoClearBackend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IAutoClearBackend() = default;

    // Schedule a one-shot task that runs at next login and clears the message.
    virtual bool schedule()  = 0;

    // Cancel / delete the scheduled task.
    virtual bool cancel()    = 0;

    // Check if a pending auto-clear task exists.
    virtual bool exists()    = 0;

    virtual QString lastError() const = 0;
};
