#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QMenuBar>

#include "TimerView.h"
#include "MessageView.h"
#include "../controllers/TimerController.h"
#include "../controllers/MessageController.h"
#include "../controllers/SettingsController.h"
#include "../core/LanguageManager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(TimerController*   timerCtrl,
                        MessageController* messageCtrl,
                        SettingsController* settingsCtrl,
                        QWidget* parent = nullptr);
    ~MainWindow();

    void showTrayIcon();

public slots:
    // Called by SingleInstanceGuard when a second instance tries to open.
    // Restores from tray/minimized, raises to front, gives focus.
    void bringToFront();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    // Menu
    void onMenuExit();
    void onMenuAbout();
    void onMenuViewReleases();
    void onMenuLanguageTriggered(QAction* action);

    // Tray
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayShowHide();
    void onTrayQuit();

    // From controllers
    void onCountdownUpdated(int remainingSeconds);
    void onRunningStateChanged(bool running);
    void onTimerError(const QString& msg);
    void onMessageCleared();
    void onMessageError(const QString& msg);
    void onLanguageApplied(AppLanguage lang);

private:
    void buildMenuBar();
    void buildTray();
    void wireControllers();
    void retranslateUI();
    static QIcon buildAppIcon();
    QString formatDuration(int seconds) const;

    // Controllers (owned externally, injected)
    TimerController*    m_timerCtrl    = nullptr;
    MessageController*  m_messageCtrl  = nullptr;
    SettingsController* m_settingsCtrl = nullptr;

    // Views (owned by MainWindow)
    TimerView*   m_timerView   = nullptr;
    MessageView* m_messageView = nullptr;
    QTabWidget*  m_tabs        = nullptr;

    // Menu bar
    QMenu*        m_settingsMenu       = nullptr;
    QMenu*        m_langSubMenu        = nullptr;
    QActionGroup* m_langActionGroup    = nullptr;
    QAction*      m_langEnAction       = nullptr;
    QAction*      m_langArAction       = nullptr;
    QAction*      m_langKoAction       = nullptr;
    QAction*      m_langEsAction       = nullptr;
    QAction*      m_langFrAction       = nullptr;
    QAction*      m_langDeAction       = nullptr;
    QAction*      m_langPtBRAction     = nullptr;
    QAction*      m_langZhCNAction     = nullptr;
    QAction*      m_langJaAction       = nullptr;
    QMenu*        m_helpMenu           = nullptr;
    QAction*      m_aboutAction        = nullptr;
    QAction*      m_viewReleasesAction = nullptr;
    QAction*      m_exitAction         = nullptr;

    // Tray
    QSystemTrayIcon* m_trayIcon           = nullptr;
    QMenu*           m_trayMenu           = nullptr;
    QAction*         m_trayShowHideAction = nullptr;
    QAction*         m_trayQuitAction     = nullptr;
    QAction*         m_trayCancelAction   = nullptr;

    bool m_forceQuit    = false;
    bool m_hiddenToTray = false;  // true when window was hidden via close/minimize-to-tray
};
