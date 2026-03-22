#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>

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
    // --execute-shutdown: fired by the LaunchAgent timer at the scheduled time.
    // Performs the actual shutdown/restart — graceful via System Events (no root),
    // or force via osascript elevation.
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
            // Force mode: skip save dialogs — requires root via osascript
            QString cmd = isShutdown ? "shutdown -h now" : "shutdown -r now";
            QProcess::execute("osascript",
                QStringList{"-e",
                    QString("do shell script \"%1\" with administrator privileges")
                    .arg(cmd)});
        } else {
            // Graceful: System Events — no root, triggers normal save dialogs,
            // same as user choosing Shut Down/Restart from the Apple menu.
            QString event = isShutdown ? "shut down" : "restart";
            QProcess::execute("osascript",
                QStringList{"-e",
                    QString("tell application \"System Events\" to %1").arg(event)});
        }
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
        // A minimal QCoreApplication is needed for QProcess and QFile on Linux.
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);

        // The systemd user service runs as a normal user — /etc/issue,
        // SDDM/LightDM/GDM configs all require root to clear.
        // Use pkexec to invoke --clear-message as root, which handles
        // all privileged file operations. The password prompt appears
        // once at login when the auto-clear fires.
        //
        // When running from an AppImage, applicationFilePath() points inside
        // the squashfs mount which is noexec — pkexec can't execute from it.
        // Use $APPIMAGE (the actual .AppImage file) when available.
        QString exePath = qEnvironmentVariable("APPIMAGE");
        if (exePath.isEmpty())
            exePath = QCoreApplication::applicationFilePath();
        QProcess::execute("pkexec", QStringList{exePath, "--clear-message"});

        // The systemd unit file lives in the user's own config directory —
        // no root needed to remove it.
        QString configHome = QStandardPaths::writableLocation(
            QStandardPaths::GenericConfigLocation);
        QString unitPath = configHome + "/systemd/user/shutdown-timer-autoclear.service";
        QProcess::execute("systemctl",
            QStringList{"--user", "disable", "shutdown-timer-autoclear.service"});
        QFile::remove(unitPath);
        // Reload systemd so it forgets the now-dangling WantedBy symlink
        QProcess::execute("systemctl", QStringList{"--user", "daemon-reload"});
    }
#elif defined(Q_OS_MACOS)
    {
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);

        // Clear the loginwindow LoginwindowText preference (primary mechanism)
        // and remove both PolicyBanner files (legacy/secondary mechanism).
        // All require root — done in a single osascript call, one password prompt.
        // Use ; between commands so each runs independently even if one fails
        // (e.g. key already deleted, or banner files already gone).
        QProcess::execute("osascript", QStringList{
            "-e",
            "do shell script \""
            "defaults delete /Library/Preferences/com.apple.loginwindow LoginwindowText"
            " ; rm -f /Library/Security/PolicyBanner.txt"
            " /Library/Security/PolicyBanner.rtf\""
            " with administrator privileges"
        });

        // Plist is in user LaunchAgents (no root needed)
        QString home = QDir::homePath();
        QString plistPath =
            home + "/Library/LaunchAgents/com.fakylab.shutdowntimer.autoclear.plist";

        // Use modern launchctl bootout (replaces deprecated unload)
        QString uid = QString::number(getuid());
        QProcess::execute("launchctl",
            QStringList{"bootout", QString("gui/%1").arg(uid), plistPath});
        QFile::remove(plistPath);
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
