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

#if defined(Q_OS_LINUX)
    // Headless write mode: called via pkexec to write message as root
    // Arguments: --write-message --title <title> --body <body>
    if (strcmp(argv[1], "--write-message") == 0) {
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);
        QString title, body, dm;
        for (int i = 2; i < argc - 1; i++) {
            if (strcmp(argv[i], "--title") == 0)      title = QString::fromUtf8(argv[++i]);
            else if (strcmp(argv[i], "--body") == 0)  body  = QString::fromUtf8(argv[++i]);
            else if (strcmp(argv[i], "--dm") == 0)    dm    = QString::fromUtf8(argv[++i]);
        }
        // Write /etc/issue
        const char* beginMarker = "# --- ShutdownTimer message begin ---";
        const char* endMarker   = "# --- ShutdownTimer message end ---";
        QFile file("/etc/issue");
        QString existing;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString c = QString::fromUtf8(file.readAll());
            file.close();
            int b = c.indexOf(beginMarker), e = c.indexOf(endMarker);
            if (b != -1 && e != -1)
                c.remove(b, (e - b) + static_cast<int>(strlen(endMarker)) + 1);
            existing = c.trimmed();
        }
        if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream out(&file);
            if (!existing.isEmpty()) out << existing << "

";
            out << beginMarker << "
";
            if (!title.isEmpty()) out << title << "
";
            if (!body.isEmpty())  out << body  << "
";
            out << endMarker << "
";
        }
        // Write DM-specific config
        if (dm == QLatin1String("sddm")) {
            QDir().mkpath("/etc/sddm.conf.d");
            QFile sddmFile("/etc/sddm.conf.d/shutdown-timer-msg.conf");
            if (sddmFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                QTextStream out(&sddmFile);
                QString combined = title.isEmpty() ? body
                    : (body.isEmpty() ? title : title + " - " + body);
                out << "[General]
WelcomeMessage=" << combined << "
";
            }
        } else if (dm == QLatin1String("lightdm")) {
            QDir().mkpath("/etc/lightdm/lightdm.conf.d");
            QFile ldmFile("/etc/lightdm/lightdm.conf.d/shutdown-timer-msg.conf");
            if (ldmFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                QTextStream out(&ldmFile);
                QString combined = title.isEmpty() ? body
                    : (body.isEmpty() ? title : title + "
" + body);
                out << "[greeter]
banner-message-enable=true
banner-message-text="
                    << combined << "
";
            }
        } else if (dm == QLatin1String("gdm")) {
            QDir().mkpath("/etc/dconf/profile");
            QDir().mkpath("/etc/dconf/db/gdm.d");
            // Ensure profile has system-db:gdm
            QFile pf("/etc/dconf/profile/gdm");
            QString pfContent;
            if (pf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                pfContent = QString::fromUtf8(pf.readAll()); pf.close();
            }
            if (!pfContent.contains("system-db:gdm")) {
                if (pf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                    QTextStream out(&pf);
                    if (!pfContent.trimmed().isEmpty()) out << pfContent.trimmed() << "
";
                    out << "system-db:gdm
";
                }
            }
            // Write banner key file
            QFile bf("/etc/dconf/db/gdm.d/01-banner-message");
            if (bf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                QTextStream out(&bf);
                QString combined = title.isEmpty() ? body
                    : (body.isEmpty() ? title : title + "
" + body);
                combined.replace("'", "\'");
                out << "[org/gnome/login-screen]
"
                    << "banner-message-enable=true
"
                    << "banner-message-text='" << combined << "'
";
            }
            QProcess::execute("dconf", QStringList{"update"});
        }
        return true;
    }

    // Headless clear mode: called via pkexec to clear message as root
    if (strcmp(argv[1], "--clear-message") == 0) {
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);
        const char* beginMarker = "# --- ShutdownTimer message begin ---";
        const char* endMarker   = "# --- ShutdownTimer message end ---";
        QFile file("/etc/issue");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString cnt = QString::fromUtf8(file.readAll());
            file.close();
            int b = cnt.indexOf(beginMarker), e = cnt.indexOf(endMarker);
            if (b != -1 && e != -1)
                cnt.remove(b, (e - b) + static_cast<int>(strlen(endMarker)) + 1);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                QTextStream out(&file);
                out << cnt.trimmed();
            }
        }
        // Remove DM configs regardless of which DM is active
        QFile::remove("/etc/sddm.conf.d/shutdown-timer-msg.conf");
        QFile::remove("/etc/lightdm/lightdm.conf.d/shutdown-timer-msg.conf");
        // GDM: remove banner key file and update dconf
        QFile::remove("/etc/dconf/db/gdm.d/01-banner-message");
        QProcess::execute("dconf", QStringList{"update"});
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
        QString exePath = QCoreApplication::applicationFilePath();
        QProcess::execute("pkexec", QStringList{exePath, "--clear-message"});

        // The systemd unit file lives in the user's own config directory —
        // no root needed to remove it.
        QString configHome = QStandardPaths::writableLocation(
            QStandardPaths::GenericConfigLocation);
        QString unitPath = configHome + "/systemd/user/shutdown-timer-autoclear.service";
        QProcess::execute("systemctl",
            QStringList{"--user", "disable", "shutdown-timer-autoclear.service"});
        QFile::remove(unitPath);
    }
#elif defined(Q_OS_MACOS)
    {
        int argcCopy = argc;
        QCoreApplication coreApp(argcCopy, argv);

        // Remove both .txt (macOS 12-) and .rtf (macOS 13+) banner files.
        // /Library/Security/ requires root — use osascript with administrator
        // privileges to remove them. This shows a password prompt if needed.
        QProcess::execute("osascript", QStringList{
            "-e",
            "do shell script "rm -f /Library/Security/PolicyBanner.txt"
            " /Library/Security/PolicyBanner.rtf""
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
