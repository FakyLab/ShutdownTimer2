#pragma once

#include <QObject>
#include <QString>

#include "../models/MessageModel.h"
#include "../services/interfaces/IMessageBackend.h"
#include "../services/interfaces/IAutoClearBackend.h"

class MessageController : public QObject
{
    Q_OBJECT
public:
    explicit MessageController(MessageModel*      model,
                               IMessageBackend*   message,
                               IAutoClearBackend* autoClear,
                               QObject*           parent = nullptr);

    const MessageModel* model() const { return m_model; }

    // Platform description string for the UI info label
    QString platformDescription() const;

    // True if message is delivered after login (notification),
    // false if shown on the login screen before login.
    bool isPostLogin() const;

public slots:
    void onSave(const QString& title, const QString& body, bool autoClear);
    void onClear();
    void onLoad();

signals:
    void messageLoaded(const QString& title, const QString& body, bool autoClearActive);
    void messageSaved(const QString& statusMsg);
    void messageCleared();
    void statusMessage(const QString& msg);
    void errorOccurred(const QString& msg);

private:
    MessageModel*      m_model;
    IMessageBackend*   m_messageBk;
    IAutoClearBackend* m_autoClearBk;
};
