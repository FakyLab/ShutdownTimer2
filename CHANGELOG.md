# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-03-21

### Added
- **Countdown Timer** — Set a duration in HH:MM:SS with quick preset buttons (15 min, 30 min, 1 hour, 2 hours)
- **Scheduled Time** — Pick an exact date and time using a calendar popup
- **Shutdown, Restart, Hibernate, Sleep** — Full action support with hardware availability detection
- **Force mode** — Skip app save prompts on Shutdown and Restart
- **Live countdown display** — Large clock with real-time tray icon tooltip updates
- **Startup Message** — Set a custom title and body on the login screen
  - Windows: `HKLM\...\Winlogon` `LegalNoticeCaption` / `LegalNoticeText`
  - macOS: `/Library/Security/PolicyBanner.txt`
  - Linux: `/etc/issue` + SDDM / LightDM config drop-ins (runtime distro detection)
- **Auto-clear** — One-shot task removes the login message after next login
  - Windows: COM Task Scheduler ONLOGON task
  - Linux: systemd user service
  - macOS: LaunchAgent plist
- **System tray** — Minimize to tray, cancel timer from tray, tray quit
- **9 languages** — English, Arabic (RTL), Korean, Spanish, French, German, Portuguese (Brazil), Chinese Simplified, Japanese
- **Persistent settings** — Window geometry and language saved via QSettings
- **MVC architecture** — Full Model-View-Controller separation with platform abstraction layer
- **Cross-platform** — Windows 10/11, Linux (systemd distros), macOS 12+
- **CMake build system** — Platform-aware source selection, auto-compiled translations

[1.0.0]: https://github.com/FakyLab/ShutdownTimer/releases/tag/v1.0.0
