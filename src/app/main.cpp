#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QFile>
#include <QTemporaryFile>
#include <QTextStream>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>

#if defined(Q_OS_LINUX)
#  include <QJsonDocument>
#  include <QJsonObject>
#  include "services/platform/linux/MessageBackendLinux.h"
#endif

#include "views/MainWindow.h"
#include "controllers/TimerController.h"
#include "controllers/MessageController.h"
#include "controllers/SettingsController.h"
#include "models/TimerModel.h"
#include "models/MessageModel.h"
#include "models/AppSettingsModel.h"
#include "core/LanguageManager.h"
#include "services/PlatformServiceFactory.h"
#include "app/SingleInstanceGuard.h"

#if defined(Q_OS_MACOS)
#  include <unistd.h>
#  include <QJsonDocument>
#  include <QJsonObject>
#  include <CoreServices/CoreServices.h>
#  include "services/platform/macos/MacOSNotifier.h"
#  include "services/platform/macos/MessageBackendMacOS.h"
#  include "services/platform/macos/AutoClearBackendMacOS.h"
#  include "services/platform/macos/ShutdownBackendMacOS.h"
#endif

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <comdef.h>
#  include <taskschd.h>
#endif

// -- Auto-clear handler --
// Runs headlessly when the scheduled logon task fires:
//   ShutdownTimer --auto-clear
// Clears the login message and removes the scheduled task, then exits.

static bool handleAutoClear(int argc, char* argv[])
{
    if (argc < 2) return false;

#if defined(Q_OS_MACOS)
    // --show-notification: fired by the notification LaunchAgent at next login.
    // Reads message.json, posts a native UNUserNotification attributed to
    // "Shutdown Timer", then handles cleanup based on the auto-clear sentinel.
    // No root required — all files are user-owned.
    if (strcmp(argv[1], "--show-notification") == 0) {
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);

        QString msgPath      = MessageBackendMacOS::messageFilePath();
        QString plistPath    = MessageBackendMacOS::notifyPlistPath();
        QString sentinelPath = AutoClearBackendMacOS::sentinelPath();

        // Read the message first — before any cleanup
        QFile f(msgPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();

            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                QString title = obj.value("title").toString().trimmed();
                QString body  = obj.value("body").toString().trimmed();

                if (!title.isEmpty() || !body.isEmpty()) {
                    // Post via UNUserNotificationCenter — attributed to
                    // "Shutdown Timer", not osascript. Requests permission
                    // on first use (one-time system dialog). Blocks until
                    // the notification is queued so it survives process exit.
                    MacOSPostNotification(title.toUtf8().constData(),
                                          body.toUtf8().constData());
                }
            }
        }

        // Remove the notify plist FILE so launchd doesn't re-load it next
        // session. Do NOT call launchctl bootout — we are the running agent,
        // and booting out self mid-execution is undefined behaviour on modern
        // launchd. Deleting the file is sufficient.
        QFile::remove(plistPath);

        // Auto-clear: if the sentinel file exists, the user chose
        // "auto-clear after next login" — delete the message now.
        // If no sentinel, the message is persistent — leave it in place
        // so the user can load it again.
        if (QFile::exists(sentinelPath)) {
            QFile::remove(msgPath);
            QFile::remove(sentinelPath);
        }

        return true;
    }

    // --execute-shutdown: fired by the LaunchAgent timer at the scheduled time.
    // Performs the actual shutdown/restart — graceful via CoreServices Apple
    // Event to kSystemProcess (no TCC, no root), or force via osascript elevation.
    // Arguments: --execute-shutdown --action shutdown|restart --force 0|1
    if (strcmp(argv[1], "--execute-shutdown") == 0) {
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);

        QString action, forceStr;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--action") == 0 && i + 1 < argc)
                action = QString::fromUtf8(argv[++i]);
            else if (strcmp(argv[i], "--force") == 0 && i + 1 < argc)
                forceStr = QString::fromUtf8(argv[++i]);
        }

        bool isShutdown = (action == QLatin1String("shutdown"));
        bool force      = (forceStr == QLatin1String("1"));

        // Remove the timer LaunchAgent plist so it doesn't fire again daily.
        // Do this BEFORE the shutdown so cleanup always happens.
        QString home      = QDir::homePath();
        QString plistPath = home +
            "/Library/LaunchAgents/com.fakylab.shutdowntimer.timer.plist";
        QString uid = QString::number(getuid());
        QProcess::execute("launchctl",
            QStringList{"bootout", QString("gui/%1").arg(uid), plistPath});
        QFile::remove(plistPath);

        if (force) {
            // Force mode: skip save dialogs — genuinely requires root.
            QString cmd = isShutdown ? "shutdown -h now" : "shutdown -r now";
            QProcess::execute("osascript",
                QStringList{"-e",
                    QString("do shell script \"%1\" with administrator privileges")
                    .arg(cmd)});
        } else {
            // Graceful: CoreServices Apple Event to kSystemProcess.
            // No TCC prompt, no root, no osascript middle-man.
            // Works headlessly from a LaunchAgent — kSystemProcess is a
            // kernel-level PSN, not a named app, so TCC does not apply.
            AEEventID eventID = isShutdown ? kAEShutDown : kAERestart;
            ShutdownBackendMacOS::sendSystemAppleEvent(eventID);
        }
        return true;
    }
#endif

#if defined(Q_OS_LINUX)
    // --show-notification: fired by the XDG autostart entry at next login.
    // Reads ~/.config/ShutdownTimer/message.json, shows a desktop notification
    // via notify-send, then removes both the JSON file and the autostart .desktop.
    // Requires no root — all files are user-owned.
    if (strcmp(argv[1], "--show-notification") == 0) {
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);

        QString msgPath     = MessageBackendLinux::messageFilePath();
        QString desktopPath = MessageBackendLinux::autostartDesktopPath();

        QFile f(msgPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();

            if (!doc.isNull() && doc.isObject()) {
                QJsonObject obj = doc.object();
                QString title = obj.value("title").toString().trimmed();
                QString body  = obj.value("body").toString().trimmed();

                if (!title.isEmpty() || !body.isEmpty()) {
                    // Small delay ensures the notification daemon is ready.
                    // XDG autostart fires very early in the session — without
                    // a brief wait, notify-send may fail on slower machines.
                    QThread::msleep(2000);

                    // notify-send arguments:
                    // --app-name    : shown in the notification header
                    // --urgency     : normal — no forced timeout
                    // --icon        : app icon if registered
                    // --expire-time : 0 = daemon decides; message stays until dismissed
                    QString summary = title.isEmpty() ? body : title;
                    QString hint    = title.isEmpty() ? QString() : body;

                    QStringList args{
                        "--app-name",    "Shutdown Timer",
                        "--urgency",     "normal",
                        "--icon",        "shutdowntimer",
                        "--expire-time", "0",
                        summary
                    };
                    if (!hint.isEmpty())
                        args << hint;

                    QProcess::execute("notify-send", args);
                }
            }
        }

        // Clean up unconditionally — stale files must not cause repeat firings
        QFile::remove(msgPath);
        QFile::remove(desktopPath);

        return true;
    }
#endif

    if (strcmp(argv[1], "--auto-clear") != 0)
        return false;

#if defined(Q_OS_WIN)
    {
        HKEY hKey = nullptr;
        LONG r = RegCreateKeyExW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
            0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE | KEY_WOW64_64KEY,
            nullptr, &hKey, nullptr);
        if (r == ERROR_SUCCESS) {
            // Write empty strings — same as MessageBackendWindows::clear().
            // Do not delete the values: they may have existed before ShutdownTimer
            // wrote to them (Group Policy, IT tools). Empty strings suppress the
            // Windows login dialog without destroying pre-existing configuration.
            const wchar_t empty[] = L"";
            DWORD bytes = sizeof(wchar_t);
            RegSetValueExW(hKey, L"LegalNoticeCaption", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(empty), bytes);
            RegSetValueExW(hKey, L"LegalNoticeText", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(empty), bytes);
            RegCloseKey(hKey);
        }
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        {
            ITaskService* pService = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_TaskScheduler, nullptr,
                                           CLSCTX_INPROC_SERVER, IID_ITaskService,
                                           reinterpret_cast<void**>(&pService)))) {
                if (SUCCEEDED(pService->Connect(_variant_t(), _variant_t(),
                                                _variant_t(), _variant_t()))) {
                    ITaskFolder* pFolder = nullptr;
                    if (SUCCEEDED(pService->GetFolder(_bstr_t(L"\\"), &pFolder))) {
                        pFolder->DeleteTask(_bstr_t(L"ShutdownTimerAutoClearMsg"), 0);
                        pFolder->Release();
                    }
                }
                pService->Release();
            }
        }
        CoUninitialize();
    }
#elif defined(Q_OS_LINUX)
    {
        // --auto-clear on Linux: entirely unprivileged.
        // The message is stored as user-owned files in ~/.config/ — no root needed.
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);

        // Remove message file and autostart entry.
        // MessageBackendLinux::clear() does exactly this — use its paths
        // to stay consistent with the interactive Clear button.
        QFile::remove(MessageBackendLinux::messageFilePath());
        QFile::remove(MessageBackendLinux::autostartDesktopPath());

        // Remove the systemd user service unit and reload.
        QString configHome = QStandardPaths::writableLocation(
            QStandardPaths::GenericConfigLocation);
        QString unitPath = configHome +
            "/systemd/user/shutdown-timer-autoclear.service";
        QProcess::execute("systemctl",
            QStringList{"--user", "disable", "shutdown-timer-autoclear.service"});
        QFile::remove(unitPath);
        QProcess::execute("systemctl", QStringList{"--user", "daemon-reload"});
    }
#elif defined(Q_OS_MACOS)
    {
        // --auto-clear on macOS.
        // The primary auto-clear path is now handled inline at the end of
        // --show-notification (which checks for the sentinel file). This
        // handler is kept as belt-and-suspenders for cases where the user
        // manually triggers a clear from the UI (MessageController::onClear
        // calls AutoClearBackendMacOS::cancel which removes the sentinel,
        // then MessageBackendMacOS::clear which removes message.json + plist).
        // Nothing to do here that isn't already handled by those calls.
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);

        // Defensive cleanup: remove any leftover files in case a previous
        // session's cleanup was interrupted.
        QFile::remove(MessageBackendMacOS::messageFilePath());
        QFile::remove(MessageBackendMacOS::notifyPlistPath());
        QFile::remove(AutoClearBackendMacOS::sentinelPath());
    }
#endif

    return true;
}

// -- Main --

int main(int argc, char* argv[])
{
    // Handle --auto-clear before Qt initializes - fast headless path
    if (handleAutoClear(argc, argv))
        return 0;

    QApplication app(argc, argv);
    app.setApplicationName("ShutdownTimer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ShutdownTimer");
    app.setQuitOnLastWindowClosed(false);

    // -- 1. Single instance guard --
    // Must be created after QApplication but before anything else.
    // If this is a second instance, it sends "activate" to the first
    // instance and we exit immediately - nothing else runs.
    SingleInstanceGuard instanceGuard("ShutdownTimer");
    if (!instanceGuard.isFirstInstance())
        return 0;

    // -- 2. Platform services --
    QObject serviceRoot;
    PlatformServices services = PlatformServiceFactory::create(&serviceRoot);

    // -- 3. Models --
    TimerModel       timerModel;
    MessageModel     messageModel;
    AppSettingsModel settingsModel;

    // -- 4. Core helpers --
    LanguageManager langMgr;

    // -- 5. Controllers --
    TimerController    timerCtrl(&timerModel, services.shutdown,
                               services.hibernateAvailable,
                               services.sleepAvailable);
    MessageController  messageCtrl(&messageModel, services.message, services.autoClear);
    SettingsController settingsCtrl(&settingsModel, &langMgr);

    // -- 6. Main window (injects controllers, creates views internally) --
    MainWindow window(&timerCtrl, &messageCtrl, &settingsCtrl);
    window.show();

    // Connect single-instance guard - when a second instance tries to open,
    // bring the existing window to the front instead.
    QObject::connect(&instanceGuard, &SingleInstanceGuard::activateRequested,
                     &window,        &MainWindow::bringToFront);

#if defined(Q_OS_WIN)
    // Force window icon via Win32 - Qt's setWindowIcon is unreliable on
    // Windows 10/11 for taskbar/title bar; the embedded exe resource is used.
    {
        HWND hwnd  = reinterpret_cast<HWND>(window.winId());
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        HICON hBig = static_cast<HICON>(
            LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON,
                       GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
                       LR_DEFAULTCOLOR));
        HICON hSmall = static_cast<HICON>(
            LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON,
                       GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                       LR_DEFAULTCOLOR));
        if (hBig)   SendMessageW(hwnd, WM_SETICON, ICON_BIG,
                                 reinterpret_cast<LPARAM>(hBig));
        if (hSmall) SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
                                 reinterpret_cast<LPARAM>(hSmall));
    }
#endif

    // Defer tray icon show until event loop is running
    QTimer::singleShot(0, &window, &MainWindow::showTrayIcon);

    return app.exec();
}
