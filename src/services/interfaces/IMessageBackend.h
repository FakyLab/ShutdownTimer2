#pragma once

#include <QObject>
#include <QString>

struct StartupMessage {
    QString title;
    QString body;
};

class IMessageBackend : public QObject
{
    Q_OBJECT
public:
    explicit IMessageBackend(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IMessageBackend() = default;

    virtual bool read(StartupMessage& out)          = 0;
    virtual bool write(const StartupMessage& msg)   = 0;
    virtual bool clear()                            = 0;

    // Human-readable description of where/when the message is displayed.
    // Used in the UI info label so users know what to expect.
    virtual QString platformDescription() const     = 0;

    // Returns true if the message is shown AFTER login (notification),
    // false if shown BEFORE login (login screen). Affects the UI label wording.
    virtual bool isPostLogin() const                { return false; }

    virtual QString lastError() const               = 0;
};
