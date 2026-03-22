#include "MessageView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

MessageView::MessageView(const QString& platformDescription, QWidget* parent)
    : QWidget(parent)
{
    buildUI(platformDescription);
}

void MessageView::buildUI(const QString& platformDescription)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    m_msgInfoLabel = new QLabel(
        tr("This message will be shown on the %1 before the user logs in.")
        .arg(platformDescription), this);
    m_msgInfoLabel->setWordWrap(true);
    layout->addWidget(m_msgInfoLabel);

    // Title
    QFormLayout* form = new QFormLayout();
    m_msgTitleLabel = new QLabel(tr("Title:"), this);
    m_msgTitleEdit  = new QLineEdit(this);
    m_msgTitleEdit->setMaxLength(80);
    m_msgTitleEdit->setPlaceholderText(tr("e.g. Important Notice"));
    form->addRow(m_msgTitleLabel, m_msgTitleEdit);
    layout->addLayout(form);

    // Body
    m_msgBodyLabel = new QLabel(tr("Message:"), this);
    layout->addWidget(m_msgBodyLabel);
    m_msgBodyEdit = new QTextEdit(this);
    m_msgBodyEdit->setAcceptRichText(false);
    m_msgBodyEdit->setMaximumHeight(120);
    m_msgBodyEdit->setPlaceholderText(
        tr("Enter your message here... (max 500 characters)"));
    layout->addWidget(m_msgBodyEdit);

    // Auto-clear
    m_autoClearCheck = new QCheckBox(tr("Auto-clear after next login"), this);
    m_autoClearCheck->setChecked(true);
    m_autoClearCheck->setToolTip(
        tr("If checked, the message will be automatically removed\n"
           "after the user logs in once."));
    layout->addWidget(m_autoClearCheck);

    // Buttons
    QHBoxLayout* btnRow = new QHBoxLayout();
    m_loadMsgBtn  = new QPushButton(tr("Load Current"), this);
    m_saveMsgBtn  = new QPushButton(tr("Save"), this);
    m_clearMsgBtn = new QPushButton(tr("Clear"), this);
    m_saveMsgBtn->setFixedHeight(32);
    m_clearMsgBtn->setFixedHeight(32);
    m_loadMsgBtn->setFixedHeight(32);
    btnRow->addWidget(m_loadMsgBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_saveMsgBtn);
    btnRow->addWidget(m_clearMsgBtn);
    layout->addLayout(btnRow);

    m_msgStatusLabel = new QLabel("", this);
    m_msgStatusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_msgStatusLabel);

    layout->addStretch();

    // -- Internal connections --
    connect(m_saveMsgBtn,  &QPushButton::clicked, this, &MessageView::onSaveClicked);
    connect(m_clearMsgBtn, &QPushButton::clicked, this, &MessageView::onClearClicked);
    connect(m_loadMsgBtn,  &QPushButton::clicked, this, &MessageView::onLoadClicked);
}

// -- Internal slots --

void MessageView::onSaveClicked()
{
    emit saveRequested(
        m_msgTitleEdit->text(),
        m_msgBodyEdit->toPlainText(),
        m_autoClearCheck->isChecked());
}

void MessageView::onClearClicked()
{
    emit clearRequested();
}

void MessageView::onLoadClicked()
{
    emit loadRequested();
}

// -- Public update methods --

void MessageView::displayMessage(const QString& title,
                                 const QString& body,
                                 bool           autoClearActive)
{
    m_msgTitleEdit->setText(title);
    m_msgBodyEdit->setPlainText(body);
    m_autoClearCheck->setChecked(autoClearActive);
}

void MessageView::showStatus(const QString& msg)
{
    m_msgStatusLabel->setText(msg);
}

void MessageView::clearFields()
{
    m_msgTitleEdit->clear();
    m_msgBodyEdit->clear();
}

void MessageView::retranslate(const QString& platformDescription)
{
    if (m_msgInfoLabel)
        m_msgInfoLabel->setText(
            tr("This message will be shown on the %1 before the user logs in.")
            .arg(platformDescription));
    if (m_msgTitleLabel)  m_msgTitleLabel->setText(tr("Title:"));
    if (m_msgBodyLabel)   m_msgBodyLabel->setText(tr("Message:"));
    if (m_msgTitleEdit)
        m_msgTitleEdit->setPlaceholderText(tr("e.g. Important Notice"));
    if (m_msgBodyEdit)
        m_msgBodyEdit->setPlaceholderText(
            tr("Enter your message here... (max 500 characters)"));
    if (m_autoClearCheck) {
        m_autoClearCheck->setText(tr("Auto-clear after next login"));
        m_autoClearCheck->setToolTip(
            tr("If checked, the message will be automatically removed\n"
               "after the user logs in once."));
    }
    if (m_saveMsgBtn)  m_saveMsgBtn->setText(tr("Save"));
    if (m_clearMsgBtn) m_clearMsgBtn->setText(tr("Clear"));
    if (m_loadMsgBtn)  m_loadMsgBtn->setText(tr("Load Current"));
}
