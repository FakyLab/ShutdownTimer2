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
    │   ├── linux/                      systemctl + ~/.config/ message file + XDG autostart
    │   └── macos/                      pmset + osascript + LaunchAgent notification
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
│       └── MessageBackendLinux
│               writes ~/.config/shutdowntimer/message.json
│               writes ~/.config/autostart/shutdowntimer-notify.desktop
│               (no root required — fully unprivileged)
│
└── macOS
    ├── ShutdownBackendMacOS     (pmset / shutdown — osascript elevation for force mode)
    ├── MessageBackendMacOS      (LaunchAgent + osascript display notification — no root)
    └── AutoClearBackendMacOS    (LaunchAgent plist)
```

---

## Linux Message System

The Linux message backend requires **no root access**. The message is stored as a user-owned JSON file and delivered via the standard XDG autostart + notify-send mechanism.

```
On write():
    ~/.config/shutdowntimer/message.json     ← stores title + body
    ~/.config/autostart/shutdowntimer-notify.desktop  ← XDG autostart entry

At next login (any freedesktop DE):
    DE session manager fires: ShutdownTimer --show-notification
    → reads message.json
    → calls notify-send (shows desktop notification)
    → deletes both files

On clear() or --auto-clear:
    deletes message.json + shutdowntimer-notify.desktop
```

This approach works on all major Linux desktop environments (GNOME, KDE, XFCE, Cinnamon, MATE, LXQt, Budgie) without requiring root, polkit, a D-Bus helper, or any system-level configuration.

---

## Timer Engine

`TimerEngine` uses a 100ms `QTimer` poll loop rather than a 1-second interval. On each tick it recalculates remaining time as `targetTime - QDateTime::currentDateTime()`. This means:

- No drift accumulates if the OS delays a tick
- The display updates smoothly at every second boundary
- Scheduled mode (fire at a specific wall-clock time) and countdown mode share identical logic

When the engine reaches zero it emits `triggered()`. `TimerController` responds by calling `IShutdownBackend::scheduleShutdown(action, 0, force)` — always with `seconds=0` since the engine fires at exactly the right moment.
