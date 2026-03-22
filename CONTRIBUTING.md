# Contributing to Shutdown Timer

Thank you for your interest in contributing! This document outlines how to get involved.

---

## Getting Started

1. **Fork** the repository on GitHub
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/FakyLab/ShutdownTimer.git
   cd ShutdownTimer
   ```
3. **Create a branch** for your change:
   ```bash
   git checkout -b feature/my-feature
   # or
   git checkout -b fix/my-bugfix
   ```
4. **Build the project** to confirm everything compiles before making changes (see [README.md](README.md#building-from-source))

---

## Project Architecture

Before contributing, please read the **Architecture** section in [README.md](README.md#architecture). This project follows strict MVC principles:

- **Models** (`src/models/`) — pure data, no UI, no platform code
- **Views** (`src/views/`) — pure Qt UI, emit signals only, never call controllers directly
- **Controllers** (`src/controllers/`) — business logic, no Qt widget imports
- **Services** (`src/services/`) — all platform-specific code lives here, behind interfaces

Please keep new code in the correct layer. A PR that adds Win32 calls to a controller or business logic to a view will be asked to refactor before merging.

---

## Types of Contributions

### Bug Reports
Please use the [Bug Report](.github/ISSUE_TEMPLATE/bug_report.md) issue template. Include:
- OS and version (e.g. Windows 11 23H2, Ubuntu 24.04, macOS 14.3)
- Steps to reproduce
- Expected vs actual behaviour
- Any relevant error messages

### Feature Requests
Please use the [Feature Request](.github/ISSUE_TEMPLATE/feature_request.md) issue template. Describe the use case clearly before describing the implementation.

### Code Contributions

#### Adding a new platform backend
1. Add your implementation files under `src/services/platform/<platform>/`
2. Implement all three interfaces: `IShutdownBackend`, `IMessageBackend`, `IAutoClearBackend`
3. Register your backends in `PlatformServiceFactory.cpp` under the correct `#ifdef`
4. Add the new source files to `CMakeLists.txt` under the correct platform block
5. Update `README.md` to document the new platform's behaviour

#### Adding a new language
1. Copy `i18n/app_en.ts` to `i18n/app_<code>.ts` (e.g. `app_it.ts` for Italian)
2. Translate all `<source>` strings into `<translation>` fields
3. Add the new `.ts` file to `CMakeLists.txt` under `TS_FILES`
4. Add the new language to `LanguageManager` (`AppLanguage` enum, `languageDisplayName`, `fromCode`, `toCode`)
5. Add the corresponding menu action in `MainWindow::buildMenuBar()`
6. Add RTL layout support in `LanguageManager::applyLanguage()` if the language is RTL

---

## Code Style

- **C++17** standard
- **Qt naming conventions** — `m_` prefix for member variables, `on` prefix for slots
- **No raw owning pointers** in new code where a parent-child Qt ownership or smart pointer is appropriate
- **No platform headers** (`windows.h`, etc.) outside of `src/services/platform/`
- All new user-facing strings must be wrapped in `tr()` for translation support
- Keep `.h` files lean — implementation in `.cpp`

---

## Submitting a Pull Request

1. Make sure the project **compiles cleanly** on your target platform with no new warnings
2. Test the feature or fix manually on at least one platform
3. Update `CHANGELOG.md` under an `[Unreleased]` section describing your change
4. Submit your PR against the `main` branch with a clear title and description

---

## Licence

By contributing, you agree that your contributions will be licensed under the [GNU GPL v3.0](LICENSE.txt).
