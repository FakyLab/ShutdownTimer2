#include "MessageController.h"

MessageController::MessageController(MessageModel*      model,
                                     IMessageBackend*   message,
                                     IAutoClearBackend* autoClear,
                                     QObject*           parent)
    : QObject(parent)
    , m_model(model)
    , m_messageBk(message)
    , m_autoClearBk(autoClear)
{}

QString MessageController::platformDescription() const
{
    return m_messageBk->platformDescription();
}

bool MessageController::isPostLogin() const
{
    return m_messageBk->isPostLogin();
}

void MessageController::onSave(const QString& title, const QString& body, bool autoClear)
{
    if (body.length() > 500) {
        emit errorOccurred(tr("Message body must not exceed 500 characters."));
        return;
    }
    if (title.trimmed().isEmpty() && body.trimmed().isEmpty()) {
        emit errorOccurred(tr("Please enter a title or message body before saving."));
        return;
    }

    StartupMessage msg;
    msg.title = title.trimmed();
    msg.body  = body.trimmed();

    if (!m_messageBk->write(msg)) {
        emit errorOccurred(tr("Failed to save message:\n%1")
                           .arg(m_messageBk->lastError()));
        return;
    }

    m_model->setMessage(msg);
    m_model->setAutoClear(autoClear);

    if (autoClear) {
        if (!m_autoClearBk->schedule()) {
            emit messageSaved(tr("Message saved, but auto-clear task failed: %1")
                              .arg(m_autoClearBk->lastError()));
            return;
        }
        emit messageSaved(tr("Message saved. Will auto-clear after next login."));
    } else {
        m_autoClearBk->cancel();
        emit messageSaved(tr("Message saved (persistent)."));
    }
}

void MessageController::onClear()
{
    if (!m_messageBk->clear()) {
        emit errorOccurred(tr("Failed to clear message:\n%1")
                           .arg(m_messageBk->lastError()));
        return;
    }

    m_autoClearBk->cancel();

    StartupMessage empty;
    m_model->setMessage(empty);
    m_model->setAutoClear(true);

    emit messageCleared();
    emit statusMessage(tr("Startup message cleared."));
}

void MessageController::onLoad()
{
    StartupMessage msg;
    if (!m_messageBk->read(msg)) {
        emit errorOccurred(tr("Failed to read message:\n%1")
                           .arg(m_messageBk->lastError()));
        return;
    }

    bool autoClearActive = m_autoClearBk->exists();
    m_model->setMessage(msg);
    m_model->setAutoClear(autoClearActive);

    emit messageLoaded(msg.title, msg.body, autoClearActive);

    if (msg.title.isEmpty() && msg.body.isEmpty())
        emit statusMessage(tr("No startup message currently set."));
    else
        emit statusMessage(tr("Loaded current startup message."));
}
