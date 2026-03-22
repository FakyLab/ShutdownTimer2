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
- **Startup Message** — Set a custom title and body on the login screen
  - Windows: `HKLM\...\Winlogon` `LegalNoticeCaption` / `LegalNoticeText`
  - macOS: `loginwindow LoginwindowText` preference (primary) + `/Library/Security/PolicyBanner.txt/.rtf` (secondary)
  - Linux (.deb / AUR): D-Bus privileged helper with PolicyKit — one password per session (`auth_admin_keep`)
  - Linux (AppImage): pkexec shell script — password per operation
  - All Linux DMs: `/etc/issue` always written as TTY fallback
  - Linux SDDM / PLM: `/etc/sddm.conf.d/shutdown-timer-msg.conf`
  - Linux LightDM: `/etc/lightdm/lightdm.conf.d/shutdown-timer-msg.conf`
  - Linux GDM: `/etc/dconf/db/gdm.d/01-banner-message` + `dconf update`
- **Auto-clear** — One-shot task removes the login message after next login
  - Windows: COM Task Scheduler ONLOGON task
  - Linux: systemd user service
  - macOS: LaunchAgent plist (`com.fakylab.shutdowntimer.autoclear`)
- **Scheduled shutdown survives app closure on macOS** — LaunchAgent registered at Start time; app can be closed after timer is set
- **System tray** — Minimize to tray, cancel timer from tray, tray quit
- **9 languages** — English, Arabic (RTL), Korean, Spanish, French, German, Portuguese (Brazil), Chinese Simplified, Japanese
- **Persistent settings** — Window geometry and language saved via QSettings
- **MVC architecture** — Full Model-View-Controller separation with platform abstraction layer
- **Cross-platform** — Windows 10/11, Linux (systemd distros), macOS 12+
- **CMake build system** — Platform-aware source selection, auto-compiled translations
- **Linux privileged helper** — `shutdowntimer-helper` D-Bus system service with polkit, installed by .deb and AUR packages
- **GitHub Actions CI/CD** — Automated builds and releases for Windows, Linux AppImage, Linux .deb, macOS Intel and Apple Silicon

### Platform notes
- macOS: app is ad-hoc signed (not notarized). On first launch use right-click → Open to bypass Gatekeeper.
- macOS: graceful shutdown/restart uses System Events (no root needed); force mode uses `shutdown` with osascript elevation.
- Linux AppImage: the privileged D-Bus helper is not installed; pkexec is used as fallback for login message writes.
- Linux .deb / AUR: the helper service is enabled and started automatically by the package install scripts.

[1.0.0]: https://github.com/FakyLab/ShutdownTimer/releases/tag/v1.0.0
