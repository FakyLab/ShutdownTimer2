# Building Shutdown Timer from Source

This document covers the full build process for all supported platforms.

---

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| Qt6 | 6.5 or newer | Must include **Qt Linguist** tools component |
| CMake | 3.20 or newer | |
| C++ compiler | C++17 capable | See platform sections below |

---

## Windows

### Toolchain
- **Qt6 with MinGW 64-bit** - download via the [Qt Online Installer](https://www.qt.io/download-qt-installer)
- When installing Qt, select:
  - `Qt 6.x > MinGW 64-bit`
  - `Qt 6.x > Additional Libraries > Qt Linguist`
  - `Developer and Designer Tools > MinGW 13.x.x 64-bit`

### Build steps

Open the **Qt 6.x MinGW 64-bit** command prompt (Start Menu -> Qt folder):

```bat
git clone https://github.com/FakyLab/ShutdownTimer.git
cd ShutdownTimer

cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Binary: `build\ShutdownTimer.exe`

### Deploying Qt runtime DLLs

To produce a standalone folder you can distribute:

```bat
mkdir deploy
windeployqt --dir deploy build\ShutdownTimer.exe
copy build\ShutdownTimer.exe deploy\
xcopy /E /I build\i18n deploy\i18n
copy LICENSE.txt deploy\
```

---

## Linux

### Dependencies

**Ubuntu / Debian:**
```bash
sudo apt-get update
sudo apt-get install cmake ninja-build \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools \
    libgl1-mesa-dev
```

**Fedora / RHEL:**
```bash
sudo dnf install cmake ninja-build \
    qt6-qtbase-devel qt6-linguist
```

**Arch Linux:**
```bash
sudo pacman -S cmake ninja qt6-base qt6-tools
```

### Build steps

```bash
git clone https://github.com/FakyLab/ShutdownTimer.git
cd ShutdownTimer

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binary: `build/ShutdownTimer`

### Running

Run the app as a normal desktop user:

```bash
./build/ShutdownTimer
```

Notes:
- Shutdown and restart may prompt for authentication depending on your distro's policy.
- Startup messages on Linux are user-scoped and unprivileged.
- Message content is stored in `~/.config/shutdowntimer/message.json`.
- Next-login display uses an XDG autostart entry in `~/.config/autostart/`.
- Auto-clear uses a `systemd --user` unit when available.

---

## macOS

### Dependencies

Install Xcode Command Line Tools:
```bash
xcode-select --install
```

Install Qt6 via Homebrew:
```bash
brew install qt6 cmake
```

Or use the [Qt Online Installer](https://www.qt.io/download-qt-installer) and select `Qt 6.x > macOS`.

### Build steps

```bash
git clone https://github.com/FakyLab/ShutdownTimer.git
cd ShutdownTimer

# If using Homebrew Qt:
export CMAKE_PREFIX_PATH=$(brew --prefix qt6)

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binary: `build/ShutdownTimer.app`

### Runtime notes

- The startup message feature is user-scoped and does not require root.
- Graceful shutdown and restart use native Apple Events.
- Force shutdown and restart may prompt for administrator authentication.

### Deploying Qt runtime

```bash
macdeployqt build/ShutdownTimer.app
# Optionally zip for distribution:
cd build && zip -r ShutdownTimer.zip ShutdownTimer.app
```

---

## Debug Build

Replace `-DCMAKE_BUILD_TYPE=Release` with `-DCMAKE_BUILD_TYPE=Debug` on any platform. Debug builds include symbols and disable optimizations.

---

## Translation Compilation

Translations are compiled automatically as part of the build. The CMake build:
1. Finds `lrelease` from your Qt installation
2. Compiles each `.ts` file in `i18n/` to `.qm`
3. Copies the `.qm` files to `build/i18n/`
4. Copies `build/i18n/` next to the final executable

To update translations after editing `.ts` files, simply re-run `cmake --build build`.

---

## Project Structure Reference

```
ShutdownTimer/
|-- src/
|   |-- app/              Entry point (main.cpp)
|   |-- core/             TimerEngine, LanguageManager
|   |-- models/           Data models (no UI, no platform code)
|   |-- controllers/      Business logic layer
|   |-- views/            Qt UI (MainWindow, TimerView, MessageView)
|   `-- services/
|       |-- interfaces/   Pure virtual backend interfaces
|       `-- platform/
|           |-- windows/  Win32 implementations
|           |-- linux/    systemctl / XDG autostart / systemd-user implementations
|           `-- macos/    Apple Events / notifications / LaunchAgent implementations
|-- i18n/                 .ts translation source files
|-- assets/               Icons (ICO + PNG at multiple sizes)
|-- CMakeLists.txt        Cross-platform build script
|-- resources.qrc         Qt resource bundle
`-- resources.rc          Windows version/icon resource (Windows only)
```
