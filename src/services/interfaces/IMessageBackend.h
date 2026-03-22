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

    // Human-readable description of where the message is displayed on this platform.
    // Used in the UI info label so users know what to expect.
    // e.g. "Windows login screen", "login screen (SDDM config + /etc/issue)", etc.
    virtual QString platformDescription() const     = 0;

    virtual QString lastError() const               = 0;
};
