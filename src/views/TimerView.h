#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QTimeEdit>
#include <QDateTimeEdit>
#include <QLabel>
#include <QStackedWidget>

#include "../services/interfaces/IShutdownBackend.h"

class TimerView : public QWidget
{
    Q_OBJECT
public:
    explicit TimerView(bool hibernateAvailable,
                       bool sleepAvailable,
                       QWidget* parent = nullptr);

    void buildUI();

    // Called by TimerController signals
    void updateCountdown(int remainingSeconds);
    void setRunningState(bool running);
    void showStatus(const QString& msg);
    void retranslate();

signals:
    // Emitted to TimerController
    void startCountdownRequested(int totalSeconds, ShutdownAction action, bool force);
    void startScheduledRequested(const QDateTime& target, ShutdownAction action, bool force);
    void cancelRequested();

private slots:
    void onStartClicked();
    void onCancelClicked();
    void onModeToggled();
    void onActionToggled();
    void onPresetClicked(int seconds);

private:
    ShutdownAction selectedAction() const;
    QString        formatDuration(int seconds) const;

    bool m_hibernateAvailable;
    bool m_sleepAvailable;

    // Mode
    QGroupBox*    m_modeGroup      = nullptr;
    QRadioButton* m_radioCountdown = nullptr;
    QRadioButton* m_radioScheduled = nullptr;

    // Input stack
    QStackedWidget* m_inputStack    = nullptr;
    QTimeEdit*      m_countdownEdit = nullptr;
    QDateTimeEdit*  m_scheduledEdit = nullptr;

    // Presets
    QPushButton* m_preset15m = nullptr;
    QPushButton* m_preset30m = nullptr;
    QPushButton* m_preset1h  = nullptr;
    QPushButton* m_preset2h  = nullptr;

    // Action
    QGroupBox*    m_actionGroup   = nullptr;
    QRadioButton* m_radioShutdown  = nullptr;
    QRadioButton* m_radioRestart   = nullptr;
    QRadioButton* m_radioHibernate = nullptr;
    QRadioButton* m_radioSleep     = nullptr;
    QCheckBox*    m_forceCheck     = nullptr;

    // Controls
    QPushButton* m_startBtn  = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    // Status
    QLabel* m_countdownLabel = nullptr;
    QLabel* m_statusLabel    = nullptr;
};
