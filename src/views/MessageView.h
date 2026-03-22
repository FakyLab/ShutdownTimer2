#pragma once

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QPushButton>

class MessageView : public QWidget
{
    Q_OBJECT
public:
    explicit MessageView(const QString& platformDescription,
                         QWidget* parent = nullptr);

    void buildUI(const QString& platformDescription);

    // Called by MessageController signals
    void displayMessage(const QString& title, const QString& body, bool autoClearActive);
    void showStatus(const QString& msg);
    void clearFields();
    void retranslate(const QString& platformDescription);

signals:
    // Emitted to MessageController
    void saveRequested(const QString& title, const QString& body, bool autoClear);
    void clearRequested();
    void loadRequested();

private slots:
    void onSaveClicked();
    void onClearClicked();
    void onLoadClicked();

private:
    QLabel*      m_msgInfoLabel   = nullptr;
    QLabel*      m_msgTitleLabel  = nullptr;
    QLabel*      m_msgBodyLabel   = nullptr;
    QLineEdit*   m_msgTitleEdit   = nullptr;
    QTextEdit*   m_msgBodyEdit    = nullptr;
    QCheckBox*   m_autoClearCheck = nullptr;
    QPushButton* m_saveMsgBtn     = nullptr;
    QPushButton* m_clearMsgBtn    = nullptr;
    QPushButton* m_loadMsgBtn     = nullptr;
    QLabel*      m_msgStatusLabel = nullptr;
};
