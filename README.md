# Shutdown Timer

A cross-platform desktop utility for scheduling system shutdowns, restarts, hibernation, and sleep — and displaying a custom message on the login screen before users log in.

💡 Ideated, directed, and shipped by [@FakyLab](https://github.com/FakyLab) — no coding knowledge required.

---

## Screenshots

| ShutdownTimer Interface | Startup Message Tab |
|:---------:|:-------------------:|
| ![Timer Tab](screenshots/screenshot_timer.png) | ![Startup Message Tab](screenshots/screenshot_message.png) |

---

## Features

- **Countdown Timer** — Set a duration (hours, minutes, seconds) and the system will perform the chosen action when it reaches zero. Quick preset buttons: 15 min, 30 min, 1 hour, 2 hours.
- **Scheduled Time** — Pick an exact date and time for the action using a calendar picker.
- **Shutdown, Restart, Hibernate, or Sleep** — Choose what happens when the timer fires. Shutdown and Restart support an optional *Force* mode that closes apps immediately without waiting for them to save. Hibernate and Sleep options are automatically greyed out if the machine doesn't support them, with a tooltip explaining why.
- **Live Countdown Display** — A large clock shows the remaining time while the timer is active. The system tray icon tooltip also updates in real time.
- **Startup Message** — Write a custom title and message body that will appear on the login screen. On Windows this uses `LegalNoticeCaption` / `LegalNoticeText` registry values. On macOS it uses `/Library/Security/PolicyBanner.txt`. On Linux it writes to the detected display manager config (SDDM, LightDM) and always to `/etc/issue` as a universal fallback.
- **Auto-Clear Message** — Optionally schedule the startup message to automatically remove itself after the next login.
- **System Tray** — The app minimizes to the system tray. Closing the window hides it rather than quitting. A tray menu lets you show/hide, cancel a running timer, or quit.
- **9 Languages** — English, Arabic (with RTL layout), Korean, Spanish, French, German, Portuguese (Brazil), Chinese Simplified, and Japanese. Language preference is saved between sessions.
- **Persistent Settings** — Window position, size, and language are all remembered across restarts.
- **Cross-platform** — Windows 10/11, Linux (SDDM/LightDM/GDM), and macOS.

---

## Architecture

This project follows **MVC (Model-View-Controller)** principles with a modular, platform-abstracted design.

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
    │   ├── linux/                      systemctl + /etc/issue + SDDM/LightDM + systemd
    │   └── macos/                      osascript/pmset + PolicyBanner + LaunchAgent
    └── PlatformServiceFactory.*        Picks correct backends at compile time
```

**Key design principles:**
- Views never call controllers directly — they only emit signals
- Controllers never import Qt widget headers — only `QObject` and signals/slots
- All platform code is isolated behind pure virtual interfaces
- `main.cpp` is the only place that knows about all layers simultaneously

---

## How It Works

### Timer

When you click **Start**, `TimerController` validates the input and delegates to `TimerEngine`, which begins a 100ms poll loop. Rather than simply counting down, it recalculates the remaining time against the wall clock on every tick — this prevents drift if the computer sleeps or the timer is delayed. When the timer reaches zero, `TimerController` calls `IShutdownBackend::scheduleShutdown()`.

### Shutdown Backend

- **Windows** — `InitiateSystemShutdownExW` for Shutdown/Restart; `SetSuspendState` for Hibernate/Sleep. Acquires `SE_SHUTDOWN_NAME` privilege first.
- **Linux** — `systemctl hibernate/suspend` for sleep states; `shutdown --poweroff/--reboot` with a delay for Shutdown/Restart.
- **macOS** — `osascript` for sleep; `pmset sleepnow` for hibernate; `shutdown -h/-r` for Shutdown/Restart.

### Startup Message

The message is written to the platform-appropriate location:

| Platform | Location |
|----------|----------|
| Windows | `HKLM\...\Winlogon` `LegalNoticeCaption` / `LegalNoticeText` |
| macOS | `/Library/Security/PolicyBanner.txt` |
| Linux (SDDM) | `/etc/sddm.conf.d/shutdown-timer-msg.conf` + `/etc/issue` |
| Linux (LightDM) | `/etc/lightdm/lightdm.conf.d/shutdown-timer-msg.conf` + `/etc/issue` |
| Linux (GDM / other) | `/etc/issue` only (GDM has no native banner support) |

**Auto-Clear** schedules a one-shot task that runs `ShutdownTimer --auto-clear` headlessly at next login, clears all written locations, removes the task itself, and exits.

| Platform | Auto-clear mechanism |
|----------|---------------------|
| Windows | COM `ITaskService` — ONLOGON task |
| Linux | systemd user service (`shutdown-timer-autoclear.service`) |
| macOS | LaunchAgent plist (`com.fakylab.shutdowntimer.autoclear`) |

> **Note:** The app requires **administrator privileges** on all platforms for registry/filesystem writes and shutdown API access.

---

## Building from Source

### Prerequisites

| Tool | Version | All platforms |
|------|---------|--------------|
| Qt6 + toolchain | 6.5 or newer | https://www.qt.io/download-qt-installer |
| CMake | 3.20 or newer | https://cmake.org/download |

**Windows:** MinGW 64-bit toolchain + Qt Linguist tools component.
**Linux:** GCC/Clang, `libqt6-dev` or equivalent, `cmake`.
**macOS:** Xcode Command Line Tools, Qt6 via Homebrew or installer.

---

### 1. Clone the repository

```bash
git clone https://github.com/FakyLab/ShutdownTimer.git
cd ShutdownTimer
```

---

### 2. Configure and build

**Windows (Qt MinGW prompt):**
```bat
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**Linux / macOS:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The compiled binary will be at `build/ShutdownTimer` (or `build/ShutdownTimer.exe` on Windows).

---

### 3. Deploy Qt runtime (Windows only)

```bat
mkdir deploy
windeployqt --dir deploy build\ShutdownTimer.exe
copy build\ShutdownTimer.exe deploy\
xcopy /E /I build\i18n deploy\i18n
```

---

## Platform Notes

### Linux
- Requires root (or `sudo`) for writing to `/etc/issue`, `/etc/sddm.conf.d/`, `/etc/lightdm/`, and calling `shutdown`.
- The auto-clear feature uses a **systemd user service** — requires systemd (Ubuntu 16.04+, Fedora, Arch, etc.).
- Display manager (SDDM/LightDM/GDM) is detected at runtime via `systemctl is-active`.
- GDM has no native login banner support — only `/etc/issue` is written on GDM systems.

### macOS
- Requires root for writing to `/Library/Security/PolicyBanner.txt` and calling `shutdown`.
- Sleep is always available; hibernate depends on `pmset hibernatemode`.
- Auto-clear uses a LaunchAgent at `/Library/LaunchAgents/`.

---

## Requirements

- **Windows** 10 or 11 (64-bit) + Administrator privileges
- **Linux** — any modern distro with systemd + root access
- **macOS** 12 Monterey or newer + root access

---

## License

This project is licensed under the **GNU General Public License v3.0** — see the [LICENSE](LICENSE.txt) file for details.

---

## Credits

- **Shutdown icons** by [Bahu Icons](https://www.flaticon.com/authors/bahu-icons) — [Flaticon](https://www.flaticon.com/free-icons/shutdown) (free for personal and commercial use with attribution)
