#include "MainWindow.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QIcon>
#include <QPixmap>
#include <QFont>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDir>

// -- App icon builder --

QIcon MainWindow::buildAppIcon()
{
    QIcon icon(":/assets/icon.ico");
    const char* pngPaths[] = {
        ":/assets/shutdown16px.png",  ":/assets/shutdown24px.png",
        ":/assets/shutdown32px.png",  ":/assets/shutdown48px.png",
        ":/assets/shutdown64px.png",  ":/assets/shutdown128px.png",
        ":/assets/shutdown256px.png", ":/assets/shutdown512px.png"
    };
    for (const char* path : pngPaths) {
        QPixmap pm(path);
        if (!pm.isNull()) icon.addPixmap(pm);
    }
    return icon;
}

// -- Constructor --

MainWindow::MainWindow(TimerController*    timerCtrl,
                       MessageController*  messageCtrl,
                       SettingsController* settingsCtrl,
                       QWidget*            parent)
    : QMainWindow(parent)
    , m_timerCtrl(timerCtrl)
    , m_messageCtrl(messageCtrl)
    , m_settingsCtrl(settingsCtrl)
{
    // Query hardware capabilities from the controller (which proxies the backend)
    bool hibernateAvailable = m_timerCtrl->isHibernateAvailable();
    bool sleepAvailable     = m_timerCtrl->isSleepAvailable();

    // Create views
    m_timerView   = new TimerView(hibernateAvailable, sleepAvailable);
    m_messageView = new MessageView(m_messageCtrl->platformDescription(),
                                    m_messageCtrl->isPostLogin());

    // Central widget + tabs
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 8);
    mainLayout->setSpacing(8);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_timerView,   tr("Timer"));
    m_tabs->addTab(m_messageView, tr("Startup Message"));
    mainLayout->addWidget(m_tabs);

    buildMenuBar();
    buildTray();
    wireControllers();
    retranslateUI();

    setWindowIcon(buildAppIcon());
    setMinimumSize(480, 520);
    resize(500, 560);

    // Restore saved language
    AppLanguage savedLang = m_settingsCtrl->currentLanguage();
    QAction* toCheck = m_langEnAction;
    if (savedLang == AppLanguage::Arabic)            toCheck = m_langArAction;
    if (savedLang == AppLanguage::Korean)            toCheck = m_langKoAction;
    if (savedLang == AppLanguage::Spanish)           toCheck = m_langEsAction;
    if (savedLang == AppLanguage::French)            toCheck = m_langFrAction;
    if (savedLang == AppLanguage::German)            toCheck = m_langDeAction;
    if (savedLang == AppLanguage::PortugueseBR)      toCheck = m_langPtBRAction;
    if (savedLang == AppLanguage::ChineseSimplified) toCheck = m_langZhCNAction;
    if (savedLang == AppLanguage::Japanese)          toCheck = m_langJaAction;
    toCheck->setChecked(true);
    m_settingsCtrl->onLanguageChanged(savedLang);

    // Restore geometry
    m_settingsCtrl->restoreWindowGeometry(this);
}

MainWindow::~MainWindow() {}

// -- Menu bar --

void MainWindow::buildMenuBar()
{
    QMenuBar* bar = menuBar();

    m_settingsMenu    = bar->addMenu(tr("Settings"));
    m_langSubMenu     = m_settingsMenu->addMenu(tr("Language"));
    m_langActionGroup = new QActionGroup(this);
    m_langActionGroup->setExclusive(true);

    auto addLang = [&](QAction*& act, AppLanguage lang) {
        act = m_langSubMenu->addAction(LanguageManager::languageDisplayName(lang));
        act->setCheckable(true);
        m_langActionGroup->addAction(act);
    };

    addLang(m_langEnAction,   AppLanguage::English);
    addLang(m_langArAction,   AppLanguage::Arabic);
    addLang(m_langKoAction,   AppLanguage::Korean);
    addLang(m_langEsAction,   AppLanguage::Spanish);
    addLang(m_langFrAction,   AppLanguage::French);
    addLang(m_langDeAction,   AppLanguage::German);
    addLang(m_langPtBRAction, AppLanguage::PortugueseBR);
    addLang(m_langZhCNAction, AppLanguage::ChineseSimplified);
    addLang(m_langJaAction,   AppLanguage::Japanese);
    m_langEnAction->setChecked(true);

    m_settingsMenu->addSeparator();
    m_exitAction = m_settingsMenu->addAction(tr("Exit"));

    m_helpMenu           = bar->addMenu(tr("Help"));
    m_aboutAction        = m_helpMenu->addAction(tr("About"));
    m_helpMenu->addSeparator();
    m_viewReleasesAction = m_helpMenu->addAction(tr("View Releases"));
}

// -- Tray --

void MainWindow::buildTray()
{
    QIcon trayIcon(":/assets/icon.ico");
    if (trayIcon.isNull())
        trayIcon = QIcon(QCoreApplication::applicationDirPath() + "/icon.ico");

    m_trayIcon = new QSystemTrayIcon(trayIcon, this);
    m_trayIcon->setToolTip(tr("Shutdown Timer"));

    m_trayMenu           = new QMenu(this);
    m_trayShowHideAction = m_trayMenu->addAction(tr("Show / Hide"));
    m_trayCancelAction   = m_trayMenu->addAction(tr("Cancel Shutdown"));
    m_trayMenu->addSeparator();
    m_trayQuitAction     = m_trayMenu->addAction(tr("Quit"));
    m_trayCancelAction->setEnabled(false);
    m_trayIcon->setContextMenu(m_trayMenu);
}

void MainWindow::showTrayIcon()
{
    if (m_trayIcon) m_trayIcon->show();
}

// -- Wire controllers to views --

void MainWindow::wireControllers()
{
    // TimerView -> TimerController
    connect(m_timerView, &TimerView::startCountdownRequested,
            m_timerCtrl, &TimerController::onStartCountdown);
    connect(m_timerView, &TimerView::startScheduledRequested,
            m_timerCtrl, &TimerController::onStartScheduled);
    connect(m_timerView, &TimerView::cancelRequested,
            m_timerCtrl, &TimerController::onCancel);

    // TimerController -> TimerView
    connect(m_timerCtrl, &TimerController::countdownUpdated,
            m_timerView, &TimerView::updateCountdown);
    connect(m_timerCtrl, &TimerController::timerStarted,
            this, [this]{ m_timerView->setRunningState(true); });
    connect(m_timerCtrl, &TimerController::timerCancelled,
            this, [this]{ m_timerView->setRunningState(false); });
    connect(m_timerCtrl, &TimerController::timerFinished,
            this, [this]{ m_timerView->setRunningState(false); });
    connect(m_timerCtrl, &TimerController::statusMessage,
            m_timerView, &TimerView::showStatus);
    connect(m_timerCtrl, &TimerController::errorOccurred,
            this, &MainWindow::onTimerError);

    // Timer running state -> tray cancel action
    connect(m_timerCtrl, &TimerController::runningStateChanged,
            this, &MainWindow::onRunningStateChanged);

    // Timer countdown -> tray tooltip
    connect(m_timerCtrl, &TimerController::countdownUpdated,
            this, &MainWindow::onCountdownUpdated);

    // MessageView -> MessageController
    connect(m_messageView, &MessageView::saveRequested,
            m_messageCtrl, &MessageController::onSave);
    connect(m_messageView, &MessageView::clearRequested,
            m_messageCtrl, &MessageController::onClear);
    connect(m_messageView, &MessageView::loadRequested,
            m_messageCtrl, &MessageController::onLoad);

    // MessageController -> MessageView
    connect(m_messageCtrl, &MessageController::messageLoaded,
            m_messageView, &MessageView::displayMessage);
    connect(m_messageCtrl, &MessageController::messageSaved,
            m_messageView, &MessageView::showStatus);
    connect(m_messageCtrl, &MessageController::messageCleared,
            this, &MainWindow::onMessageCleared);
    connect(m_messageCtrl, &MessageController::statusMessage,
            m_messageView, &MessageView::showStatus);
    connect(m_messageCtrl, &MessageController::errorOccurred,
            this, &MainWindow::onMessageError);

    // Menu bar
    connect(m_exitAction,         &QAction::triggered,
            this, &MainWindow::onMenuExit);
    connect(m_aboutAction,        &QAction::triggered,
            this, &MainWindow::onMenuAbout);
    connect(m_viewReleasesAction, &QAction::triggered,
            this, &MainWindow::onMenuViewReleases);
    connect(m_langActionGroup,    &QActionGroup::triggered,
            this, &MainWindow::onMenuLanguageTriggered);

    // Tray
    connect(m_trayIcon,           &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);
    connect(m_trayShowHideAction, &QAction::triggered,
            this, &MainWindow::onTrayShowHide);
    connect(m_trayQuitAction,     &QAction::triggered,
            this, &MainWindow::onTrayQuit);
    connect(m_trayCancelAction,   &QAction::triggered,
            m_timerCtrl, &TimerController::onCancel);

    // Language
    connect(m_settingsCtrl, &SettingsController::languageApplied,
            this, &MainWindow::onLanguageApplied);
}

// -- Controller response slots --

void MainWindow::onCountdownUpdated(int remainingSeconds)
{
    m_trayIcon->setToolTip(
        tr("Shutdown Timer — %1").arg(formatDuration(remainingSeconds)));
}

void MainWindow::onRunningStateChanged(bool running)
{
    m_trayCancelAction->setEnabled(running);
}

void MainWindow::onTimerError(const QString& msg)
{
    QMessageBox::critical(this, tr("Error"), msg);
}

void MainWindow::onMessageCleared()
{
    m_messageView->clearFields();
}

void MainWindow::onMessageError(const QString& msg)
{
    QMessageBox::critical(this, tr("Error"), msg);
}

// -- Menu slots --

void MainWindow::onMenuExit()
{
    if (m_timerCtrl->model()->isRunning()) {
        auto reply = QMessageBox::question(
            this, tr("Exit"),
            tr("A shutdown timer is running. Exit anyway?\n"
               "This will cancel the pending shutdown."),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }

    if (m_timerCtrl->model()->isRunning())
        m_timerCtrl->onCancel();

    if (m_trayIcon) m_trayIcon->hide();
    m_forceQuit = true;
    qApp->quit();
}

void MainWindow::onMenuAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(tr("About Shutdown Timer"));
    box.setText(
        tr("Shutdown Timer — version %1\n\n"
           "A cross-platform utility for scheduling system shutdowns and restarts, "
           "and setting a message on the login screen before users log in.\n\n"
           "© 2026 Shutdown Timer\nFakyLab")
        .arg(QCoreApplication::applicationVersion()));
    box.setIconPixmap(QPixmap(":/assets/shutdown64px.png"));

    QPushButton* licenseBtn = box.addButton(tr("View License"), QMessageBox::HelpRole);
    box.addButton(QMessageBox::Ok);
    box.setDefaultButton(QMessageBox::Ok);
    box.exec();

    if (box.clickedButton() == licenseBtn) {
        QString licensePath =
            QCoreApplication::applicationDirPath() + "/LICENSE.txt";
        if (!QFile::exists(licensePath))
            licensePath = QDir::currentPath() + "/LICENSE.txt";

        if (QFile::exists(licensePath))
            QDesktopServices::openUrl(QUrl::fromLocalFile(licensePath));
        else
            QMessageBox::information(this, tr("License"),
                tr("LICENSE.txt was not found.\n"
                   "You can read the full GNU GPL v3 at:\n"
                   "https://www.gnu.org/licenses/gpl-3.0.html"));
    }
}

void MainWindow::onMenuViewReleases()
{
    QDesktopServices::openUrl(
        QUrl("https://github.com/FakyLab/ShutdownTimer/releases"));
}

void MainWindow::onMenuLanguageTriggered(QAction* action)
{
    AppLanguage lang = AppLanguage::English;
    if (action == m_langArAction)   lang = AppLanguage::Arabic;
    if (action == m_langKoAction)   lang = AppLanguage::Korean;
    if (action == m_langEsAction)   lang = AppLanguage::Spanish;
    if (action == m_langFrAction)   lang = AppLanguage::French;
    if (action == m_langDeAction)   lang = AppLanguage::German;
    if (action == m_langPtBRAction) lang = AppLanguage::PortugueseBR;
    if (action == m_langZhCNAction) lang = AppLanguage::ChineseSimplified;
    if (action == m_langJaAction)   lang = AppLanguage::Japanese;

    m_settingsCtrl->onLanguageChanged(lang);
}

// -- Tray slots --

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger ||
        reason == QSystemTrayIcon::DoubleClick)
        onTrayShowHide();
}

void MainWindow::onTrayShowHide()
{
    if (isVisible() && !isMinimized()) {
        m_hiddenToTray = true;
        hide();
    } else {
        bringToFront();
    }
}

void MainWindow::onTrayQuit()
{
    if (m_timerCtrl->model()->isRunning()) {
        auto reply = QMessageBox::question(
            this, tr("Quit"),
            tr("A shutdown timer is running. Quit anyway?\n"
               "(The OS shutdown will still proceed if already initiated.)"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }

    if (m_trayIcon) m_trayIcon->hide();
    m_forceQuit = true;
    qApp->quit();
}

// -- Language --

void MainWindow::onLanguageApplied(AppLanguage /*lang*/)
{
    retranslateUI();
}

// -- Close / minimize --

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!m_forceQuit) {
        m_settingsCtrl->onWindowGeometryChanged(saveGeometry());
        m_hiddenToTray = true;
        hide();
        m_trayIcon->showMessage(
            tr("Shutdown Timer"),
            tr("Running in the system tray. Double-click the icon to restore."),
            QSystemTrayIcon::Information, 3000);
        event->ignore();
    } else {
        m_settingsCtrl->onWindowGeometryChanged(saveGeometry());
        event->accept();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            m_hiddenToTray = true;
            QTimer::singleShot(0, this, &QWidget::hide);
        }
    } else if (event->type() == QEvent::LanguageChange) {
        retranslateUI();
    }
    QMainWindow::changeEvent(event);
}

// -- Retranslate --

void MainWindow::retranslateUI()
{
    setWindowTitle(tr("Shutdown Timer"));

    if (m_settingsMenu)       m_settingsMenu->setTitle(tr("Settings"));
    if (m_langSubMenu)        m_langSubMenu->setTitle(tr("Language"));
    if (m_exitAction)         m_exitAction->setText(tr("Exit"));
    if (m_helpMenu)           m_helpMenu->setTitle(tr("Help"));
    if (m_aboutAction)        m_aboutAction->setText(tr("About"));
    if (m_viewReleasesAction) m_viewReleasesAction->setText(tr("View Releases"));

    if (m_tabs && m_tabs->count() >= 2) {
        m_tabs->setTabText(0, tr("Timer"));
        m_tabs->setTabText(1, tr("Startup Message"));
    }

    if (m_trayShowHideAction) m_trayShowHideAction->setText(tr("Show / Hide"));
    if (m_trayCancelAction)   m_trayCancelAction->setText(tr("Cancel Shutdown"));
    if (m_trayQuitAction)     m_trayQuitAction->setText(tr("Quit"));
    if (m_trayIcon)           m_trayIcon->setToolTip(tr("Shutdown Timer"));

    if (m_timerView)   m_timerView->retranslate();
    if (m_messageView) m_messageView->retranslate(m_messageCtrl->platformDescription(),
                                                   m_messageCtrl->isPostLogin());
}

// -- Single instance --

void MainWindow::bringToFront()
{
    // Clear the tray-hidden flag so the balloon doesn't fire again
    m_hiddenToTray = false;

    // Restore from tray or minimized state
    if (!isVisible()) {
        // Window was hidden to tray - show it in normal state
        setWindowState(windowState() & ~Qt::WindowMinimized);
        show();
    } else if (isMinimized()) {
        // Window is minimized but still visible in taskbar
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }

    // Bring to front and give focus
    raise();
    activateWindow();
}

// -- Helpers --

QString MainWindow::formatDuration(int seconds) const
{
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    if (h > 0) return QString("%1h %2m %3s").arg(h).arg(m).arg(s);
    if (m > 0) return QString("%1m %2s").arg(m).arg(s);
    return QString("%1s").arg(s);
}
