# Architecture

Shutdown Timer follows **MVC (Model-View-Controller)** principles with a modular, platform-abstracted design. All platform-specific code is isolated behind pure virtual interfaces, making each layer independently testable and replaceable.

---

## Source Tree

```
src/
├── app/
│   └── main.cpp                        Entry point — wires all layers together
│
├── core/
│   ├── TimerEngine.*                   Drift-corrected countdown/scheduled timer
│   └── LanguageManager.*               QTranslator + RTL switching
│
├── models/
│   ├── TimerModel.*                    Timer state (mode, action, remaining, running)
│   ├── MessageModel.*                  Startup message state
│   └── AppSettingsModel.*              Persistent settings (language, geometry)
│
├── controllers/
│   ├── TimerController.*               Drives TimerEngine, calls shutdown backend
│   ├── MessageController.*             Reads/writes message via backend
│   └── SettingsController.*            Applies language, saves geometry
│
├── views/
│   ├── MainWindow.*                    Thin shell — tabs, menu bar, tray
│   ├── TimerView.*                     Timer tab UI — emits signals only
│   └── MessageView.*                   Message tab UI — emits signals only
│
└── services/
    ├── interfaces/
    │   ├── IShutdownBackend.h          Pure virtual: scheduleShutdown / cancel
    │   ├── IMessageBackend.h           Pure virtual: read / write / clear
    │   └── IAutoClearBackend.h         Pure virtual: schedule / cancel / exists
    ├── platform/
    │   ├── windows/                    Win32 + Registry + COM Task Scheduler
    │   ├── linux/                      systemctl + /etc/issue + DM configs + D-Bus helper
    │   └── macos/                      pmset + defaults + PolicyBanner + LaunchAgent
    └── PlatformServiceFactory.*        Picks correct backends at runtime/compile time
```

---

## Design Principles

- **Views emit signals only** — they never call controllers directly and have no business logic
- **Controllers are widget-free** — they only import `QObject` and communicate via signals/slots
- **Platform code is fully isolated** — all OS-specific behaviour lives behind `IShutdownBackend`, `IMessageBackend`, and `IAutoClearBackend`; the rest of the app never sees a `#ifdef`
- **`main.cpp` is the composition root** — the only file that knows about all layers simultaneously; it constructs everything and injects dependencies

---

## Layer Interactions

```
TimerView  ──signal──►  TimerController  ──►  TimerEngine  ──►  IShutdownBackend
                              │
                         (countdownUpdated)
                              │
                              ▼
                         MainWindow  ──►  tray tooltip / tray cancel button

MessageView  ──signal──►  MessageController  ──►  IMessageBackend
                                                   IAutoClearBackend

SettingsController  ──►  LanguageManager  ──►  QTranslator  ──►  qApp
                    ──►  AppSettingsModel  ──►  QSettings
```

---

## Platform Backend Selection

`PlatformServiceFactory::create()` is called once at startup. On Linux it also performs runtime DM detection:

```
PlatformServiceFactory::create()
│
├── Windows
│   ├── ShutdownBackendWindows   (InitiateSystemShutdownExW / SetSuspendState)
│   ├── MessageBackendWindows    (HKLM Winlogon registry)
│   └── AutoClearBackendWindows  (COM ITaskService ONLOGON task)
│
├── Linux
│   ├── ShutdownBackendLinux     (systemctl / shutdown)
│   ├── AutoClearBackendLinux    (systemd user service)
│   └── message backend — selected at runtime:
│       ├── isHelperAvailable()?
│       │   YES → MessageBackendLinuxDBus   (D-Bus → polkit helper, once per session)
│       │   NO  → MessageBackendLinux       (direct write + pkexec shell script fallback)
│       └── DM detected via systemctl is-active:
│           SDDM / PLM → /etc/sddm.conf.d/
│           LightDM    → /etc/lightdm/lightdm.conf.d/
│           GDM        → /etc/dconf/db/gdm.d/ + dconf update
│           fallback   → /etc/issue only
│
└── macOS
    ├── ShutdownBackendMacOS     (pmset / shutdown)
    ├── MessageBackendMacOS      (loginwindow defaults + PolicyBanner)
    └── AutoClearBackendMacOS    (LaunchAgent plist)
```

---

## Linux Privileged Helper

On `.deb` and AUR installs, a separate C++ binary (`shutdowntimer-helper`) is installed at `/usr/libexec/`. It runs as root via D-Bus system bus activation and is the only component that writes to `/etc/issue`, DM config files, and dconf. All methods are protected by a PolicyKit action defined in `linux/polkit/org.fakylab.shutdowntimer.policy`.

```
GUI app (unprivileged)
    │
    │  D-Bus call: WriteMessage(title, body, dm)
    ▼
shutdowntimer-helper (root, system bus)
    │
    ├── checkAuthorization("org.fakylab.shutdowntimer.write-message")
    │       └── polkit shows password dialog if needed
    │           auth_admin_keep: remembered for the session
    │
    └── writes: /etc/issue
                /etc/sddm.conf.d/   (SDDM/PLM)
                /etc/lightdm/       (LightDM)
                /etc/dconf/         (GDM) + dconf update
```

On AppImage installs where the helper is not present, `MessageBackendLinux` falls back to writing a temporary shell script to `/tmp` (random name, `chmod 700`) and running it via `pkexec /bin/sh`. This bypasses the AppImage FUSE/noexec mount restriction.

---

## Timer Engine

`TimerEngine` uses a 100ms `QTimer` poll loop rather than a 1-second interval. On each tick it recalculates remaining time as `targetTime - QDateTime::currentDateTime()`. This means:

- No drift accumulates if the OS delays a tick
- The display updates smoothly at every second boundary
- Scheduled mode (fire at a specific wall-clock time) and countdown mode share identical logic

When the engine reaches zero it emits `triggered()`. `TimerController` responds by calling `IShutdownBackend::scheduleShutdown(action, 0, force)` — always with `seconds=0` since the engine fires at exactly the right moment.
