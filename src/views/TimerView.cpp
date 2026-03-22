#include "TimerView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>

TimerView::TimerView(bool hibernateAvailable, bool sleepAvailable, QWidget* parent)
    : QWidget(parent)
    , m_hibernateAvailable(hibernateAvailable)
    , m_sleepAvailable(sleepAvailable)
{
    buildUI();
}

void TimerView::buildUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // -- Mode group --
    m_modeGroup = new QGroupBox(tr("Timer Mode"), this);
    QHBoxLayout* modeLayout = new QHBoxLayout(m_modeGroup);
    m_radioCountdown = new QRadioButton(tr("Countdown"), m_modeGroup);
    m_radioScheduled = new QRadioButton(tr("Scheduled Time"), m_modeGroup);
    m_radioCountdown->setChecked(true);
    modeLayout->addWidget(m_radioCountdown);
    modeLayout->addWidget(m_radioScheduled);
    modeLayout->addStretch();
    layout->addWidget(m_modeGroup);

    // -- Input stack --
    m_inputStack = new QStackedWidget(this);

    // Page 0: Countdown
    QWidget* countdownPage = new QWidget();
    QVBoxLayout* cdLayout = new QVBoxLayout(countdownPage);
    cdLayout->setContentsMargins(0, 4, 0, 4);
    cdLayout->setSpacing(8);

    m_countdownEdit = new QTimeEdit(QTime(0, 15, 0), countdownPage);
    m_countdownEdit->setDisplayFormat("HH : mm : ss");
    m_countdownEdit->setFixedHeight(52);
    m_countdownEdit->setFixedWidth(220);
    QFont teFont = m_countdownEdit->font();
    teFont.setPointSize(18);
    m_countdownEdit->setFont(teFont);
    m_countdownEdit->setAlignment(Qt::AlignCenter);
    // Hide the up/down arrow buttons — user edits directly by clicking
    // a section and typing, or uses the scroll wheel over each field.
    m_countdownEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    // Remove the spin box up/down arrows via stylesheet — setButtonSymbols
    // alone does not remove them on all Windows styles.
    m_countdownEdit->setStyleSheet(
        "QTimeEdit {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  background: palette(base);"
        "  padding: 6px 12px;"
        "  padding-right: 0px;"
        "}"
        "QTimeEdit::up-button { width: 0px; height: 0px; border: none; }"
        "QTimeEdit::down-button { width: 0px; height: 0px; border: none; }"
    );
    m_countdownEdit->setToolTip(
        tr("Click hours, minutes, or seconds and type a value,\n"
           "or scroll the mouse wheel to adjust."));

    QHBoxLayout* timeRow = new QHBoxLayout();
    timeRow->addStretch();
    timeRow->addWidget(m_countdownEdit);
    timeRow->addStretch();
    cdLayout->addLayout(timeRow);

    m_preset15m = new QPushButton(tr("15 min"), countdownPage);
    m_preset30m = new QPushButton(tr("30 min"), countdownPage);
    m_preset1h  = new QPushButton(tr("1 hour"), countdownPage);
    m_preset2h  = new QPushButton(tr("2 hours"), countdownPage);

    for (QPushButton* btn : {m_preset15m, m_preset30m, m_preset1h, m_preset2h}) {
        btn->setFixedHeight(28);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
    }

    QHBoxLayout* presetRow = new QHBoxLayout();
    presetRow->addStretch();
    presetRow->addWidget(m_preset15m);
    presetRow->addWidget(m_preset30m);
    presetRow->addWidget(m_preset1h);
    presetRow->addWidget(m_preset2h);
    presetRow->addStretch();
    cdLayout->addLayout(presetRow);
    m_inputStack->addWidget(countdownPage);

    // Page 1: Scheduled
    QWidget* scheduledPage = new QWidget();
    QHBoxLayout* scLayout = new QHBoxLayout(scheduledPage);
    scLayout->setContentsMargins(0, 0, 0, 0);
    m_scheduledEdit = new QDateTimeEdit(
        QDateTime::currentDateTime().addSecs(3600), scheduledPage);
    m_scheduledEdit->setDisplayFormat("yyyy-MM-dd  HH:mm:ss");
    m_scheduledEdit->setCalendarPopup(true);
    m_scheduledEdit->setMinimumDateTime(QDateTime::currentDateTime());
    scLayout->addStretch();
    scLayout->addWidget(m_scheduledEdit);
    scLayout->addStretch();
    m_inputStack->addWidget(scheduledPage);

    layout->addWidget(m_inputStack);

    // -- Action group --
    m_actionGroup = new QGroupBox(tr("Action"), this);
    QVBoxLayout* actionLayout = new QVBoxLayout(m_actionGroup);
    QHBoxLayout* actionRow    = new QHBoxLayout();

    m_radioShutdown  = new QRadioButton(tr("Shutdown"),  m_actionGroup);
    m_radioRestart   = new QRadioButton(tr("Restart"),   m_actionGroup);
    m_radioHibernate = new QRadioButton(tr("Hibernate"), m_actionGroup);
    m_radioSleep     = new QRadioButton(tr("Sleep"),     m_actionGroup);
    m_radioShutdown->setChecked(true);

    actionRow->addWidget(m_radioShutdown);
    actionRow->addWidget(m_radioRestart);
    actionRow->addWidget(m_radioHibernate);
    actionRow->addWidget(m_radioSleep);
    actionRow->addStretch();

    if (!m_hibernateAvailable) {
        m_radioHibernate->setEnabled(false);
        m_radioHibernate->setToolTip(
            tr("Hibernate is not available on this machine.\n"
               "It may be disabled via: powercfg /h on"));
    }
    if (!m_sleepAvailable) {
        m_radioSleep->setEnabled(false);
        m_radioSleep->setToolTip(tr("Sleep is not available on this machine."));
    }

    m_forceCheck = new QCheckBox(
        tr("Force (don't wait for apps to close)"), m_actionGroup);
    actionLayout->addLayout(actionRow);
    actionLayout->addWidget(m_forceCheck);
    layout->addWidget(m_actionGroup);

    // -- Countdown display --
    m_countdownLabel = new QLabel("--:--:--", this);
    QFont f = m_countdownLabel->font();
    f.setPointSize(32);
    f.setBold(true);
    m_countdownLabel->setFont(f);
    m_countdownLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_countdownLabel);

    m_statusLabel = new QLabel("", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    // -- Buttons --
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_startBtn  = new QPushButton(tr("Start"), this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_startBtn->setFixedHeight(36);
    m_cancelBtn->setFixedHeight(36);
    m_cancelBtn->setEnabled(false);
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_cancelBtn);
    layout->addLayout(btnLayout);
    layout->addStretch();

    // -- Connections (internal only) --
    connect(m_startBtn,       &QPushButton::clicked,
            this,             &TimerView::onStartClicked);
    connect(m_cancelBtn,      &QPushButton::clicked,
            this,             &TimerView::onCancelClicked);
    connect(m_radioCountdown, &QRadioButton::toggled,
            this,             &TimerView::onModeToggled);
    connect(m_radioScheduled, &QRadioButton::toggled,
            this,             &TimerView::onModeToggled);
    connect(m_radioShutdown,  &QRadioButton::toggled,
            this,             &TimerView::onActionToggled);
    connect(m_radioRestart,   &QRadioButton::toggled,
            this,             &TimerView::onActionToggled);
    connect(m_radioHibernate, &QRadioButton::toggled,
            this,             &TimerView::onActionToggled);
    connect(m_radioSleep,     &QRadioButton::toggled,
            this,             &TimerView::onActionToggled);

    connect(m_preset15m, &QPushButton::clicked,
            this, [this]{ onPresetClicked(15 * 60); });
    connect(m_preset30m, &QPushButton::clicked,
            this, [this]{ onPresetClicked(30 * 60); });
    connect(m_preset1h,  &QPushButton::clicked,
            this, [this]{ onPresetClicked(60 * 60); });
    connect(m_preset2h,  &QPushButton::clicked,
            this, [this]{ onPresetClicked(120 * 60); });
}

// -- Internal slots --

void TimerView::onStartClicked()
{
    ShutdownAction action = selectedAction();
    bool force = m_forceCheck->isChecked();

    if (m_radioCountdown->isChecked()) {
        QTime t = m_countdownEdit->time();
        int totalSeconds = t.hour() * 3600 + t.minute() * 60 + t.second();
        emit startCountdownRequested(totalSeconds, action, force);
    } else {
        emit startScheduledRequested(m_scheduledEdit->dateTime(), action, force);
    }
}

void TimerView::onCancelClicked()
{
    emit cancelRequested();
}

void TimerView::onModeToggled()
{
    m_inputStack->setCurrentIndex(m_radioScheduled->isChecked() ? 1 : 0);
}

void TimerView::onActionToggled()
{
    bool isSuspend = m_radioHibernate->isChecked() || m_radioSleep->isChecked();
    m_forceCheck->setEnabled(!isSuspend);
    if (isSuspend)
        m_forceCheck->setChecked(false);
}

void TimerView::onPresetClicked(int seconds)
{
    m_countdownEdit->setTime(
        QTime(seconds / 3600, (seconds % 3600) / 60, seconds % 60));
}

// -- Public update slots --

void TimerView::updateCountdown(int remainingSeconds)
{
    int h = remainingSeconds / 3600;
    int m = (remainingSeconds % 3600) / 60;
    int s = remainingSeconds % 60;
    m_countdownLabel->setText(
        QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')));
}

void TimerView::setRunningState(bool running)
{
    m_startBtn->setEnabled(!running);
    m_cancelBtn->setEnabled(running);
    m_radioCountdown->setEnabled(!running);
    m_radioScheduled->setEnabled(!running);
    m_countdownEdit->setEnabled(!running);
    m_scheduledEdit->setEnabled(!running);
    m_preset15m->setEnabled(!running);
    m_preset30m->setEnabled(!running);
    m_preset1h->setEnabled(!running);
    m_preset2h->setEnabled(!running);
    m_radioShutdown->setEnabled(!running);
    m_radioRestart->setEnabled(!running);

    if (m_hibernateAvailable) m_radioHibernate->setEnabled(!running);
    if (m_sleepAvailable)     m_radioSleep->setEnabled(!running);

    bool isSuspend = m_radioHibernate->isChecked() || m_radioSleep->isChecked();
    m_forceCheck->setEnabled(!running && !isSuspend);

    if (!running)
        m_countdownLabel->setText("--:--:--");
}

void TimerView::showStatus(const QString& msg)
{
    m_statusLabel->setText(msg);
}

// -- Helpers --

ShutdownAction TimerView::selectedAction() const
{
    if (m_radioRestart->isChecked())   return ShutdownAction::Restart;
    if (m_radioHibernate->isChecked()) return ShutdownAction::Hibernate;
    if (m_radioSleep->isChecked())     return ShutdownAction::Sleep;
    return ShutdownAction::Shutdown;
}

QString TimerView::formatDuration(int seconds) const
{
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    if (h > 0) return QString("%1h %2m %3s").arg(h).arg(m).arg(s);
    if (m > 0) return QString("%1m %2s").arg(m).arg(s);
    return QString("%1s").arg(s);
}

void TimerView::retranslate()
{
    if (m_modeGroup)      m_modeGroup->setTitle(tr("Timer Mode"));
    if (m_radioCountdown) m_radioCountdown->setText(tr("Countdown"));
    if (m_radioScheduled) m_radioScheduled->setText(tr("Scheduled Time"));
    if (m_actionGroup)    m_actionGroup->setTitle(tr("Action"));
    if (m_radioShutdown)  m_radioShutdown->setText(tr("Shutdown"));
    if (m_radioRestart)   m_radioRestart->setText(tr("Restart"));
    if (m_radioHibernate) m_radioHibernate->setText(tr("Hibernate"));
    if (m_radioSleep)     m_radioSleep->setText(tr("Sleep"));
    if (m_forceCheck)
        m_forceCheck->setText(tr("Force (don't wait for apps to close)"));
    if (m_startBtn)  m_startBtn->setText(tr("Start"));
    if (m_cancelBtn) m_cancelBtn->setText(tr("Cancel"));
    if (m_preset15m) m_preset15m->setText(tr("15 min"));
    if (m_preset30m) m_preset30m->setText(tr("30 min"));
    if (m_preset1h)  m_preset1h->setText(tr("1 hour"));
    if (m_preset2h)  m_preset2h->setText(tr("2 hours"));
}
