#pragma once

#include <QObject>
#include <QString>
#include "../services/interfaces/IMessageBackend.h"

class MessageModel : public QObject
{
    Q_OBJECT
public:
    explicit MessageModel(QObject* parent = nullptr);

    const StartupMessage& currentMessage() const { return m_message; }
    bool                  autoClear()      const { return m_autoClear; }

    void setMessage(const StartupMessage& msg) { m_message = msg; }
    void setAutoClear(bool v)                  { m_autoClear = v; }

private:
    StartupMessage m_message;
    bool           m_autoClear = true;
};
