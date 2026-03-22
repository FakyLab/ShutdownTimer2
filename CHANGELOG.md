# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-03-22

### Added
- **Countdown Timer** — Set a duration in HH:MM:SS with quick preset buttons (15 min, 30 min, 1 hour, 2 hours)
- **Scheduled Time** — Pick an exact date and time using a calendar popup
- **Shutdown, Restart, Hibernate, Sleep** — Full action support with hardware availability detection
- **Force mode** — Skip app save prompts on Shutdown and Restart (Windows and macOS)
- **Live countdown display** — Large clock with real-time tray icon tooltip updates
- **Startup Message** — Set a custom title and body delivered to the user at next login
  - Windows: `HKLM\...\Winlogon` `LegalNoticeCaption` / `LegalNoticeText` (login screen)
  - macOS: `loginwindow LoginwindowText` preference + PolicyBanner files (login screen)
  - Linux: XDG autostart + `notify-send` desktop notification — **no root required**, works on all major DEs (GNOME, KDE, XFCE, Cinnamon, MATE, LXQt, Budgie)
- **Auto-clear** — One-shot task removes the login message after next login
  - Windows: COM Task Scheduler ONLOGON task
  - Linux: systemd user service (fully unprivileged)
  - macOS: LaunchAgent plist (`com.fakylab.shutdowntimer.autoclear`)
- **Scheduled shutdown survives app closure on macOS** — LaunchAgent registered at Start time
- **System tray** — Minimize to tray, cancel timer from tray, tray quit
- **9 languages** — English, Arabic (RTL), Korean, Spanish, French, German, Portuguese (Brazil), Chinese Simplified, Japanese
- **Persistent settings** — Window geometry and language saved via QSettings
- **MVC architecture** — Full Model-View-Controller separation with platform abstraction layer
- **Cross-platform** — Windows 10/11, Linux (any freedesktop DE with systemd), macOS 12+
- **CMake build system** — Platform-aware source selection, auto-compiled translations
- **GitHub Actions CI/CD** — Automated builds and releases for Windows, Linux AppImage, Linux .deb, macOS Intel and Apple Silicon

### Platform notes
- macOS: app is ad-hoc signed (not notarized). On first launch use right-click → Open to bypass Gatekeeper.
- macOS: graceful shutdown/restart uses System Events (no root needed); force mode uses `shutdown` with osascript elevation.
- Linux: the message feature is fully unprivileged — no root, no polkit, no D-Bus helper required.

[1.0.0]: https://github.com/FakyLab/ShutdownTimer/releases/tag/v1.0.0
