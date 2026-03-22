#pragma once

#include "../../interfaces/IMessageBackend.h"

// macOS login screen message — two complementary mechanisms:
//
// PRIMARY: loginwindow LoginwindowText preference
//   defaults write /Library/Preferences/com.apple.loginwindow LoginwindowText "..."
//   This is Apple's intended API for login window messages. It shows as a
//   subtitle under the machine name on the login screen. Works on all macOS
//   versions including Sequoia. Requires root to write to /Library/Preferences/.
//
// SECONDARY (legacy/compliance): PolicyBanner files
//   /Library/Security/PolicyBanner.txt  — macOS 12 and earlier
//   /Library/Security/PolicyBanner.rtf  — macOS 13+ (Ventura+)
//   Shows a full-screen consent banner that the user must dismiss before
//   logging in. Used for legal/compliance notices. Also requires root.
//
// Both are written together on save and cleared together on clear.
// Elevation is done via a single osascript call (one password prompt).

class MessageBackendMacOS : public IMessageBackend
{
    Q_OBJECT
public:
    explicit MessageBackendMacOS(QObject* parent = nullptr);

    bool read(StartupMessage& out)        override;
    bool write(const StartupMessage& msg) override;
    bool clear()                          override;

    QString platformDescription() const   override;
    QString lastError() const             override { return m_lastError; }

private:
    // Run a shell command with root privileges via osascript.
    // Shows a native macOS password dialog. Returns true on success.
    bool runElevated(const QString& shellCmd);

    QString m_lastError;

    // loginwindow defaults — primary mechanism
    static constexpr char kLoginwindowPlist[] =
        "/Library/Preferences/com.apple.loginwindow";
    static constexpr char kLoginwindowKey[]   = "LoginwindowText";

    // PolicyBanner — secondary/legacy mechanism
    static constexpr char kBannerPath[]    = "/Library/Security/PolicyBanner.txt";
    static constexpr char kBannerPathRtf[] = "/Library/Security/PolicyBanner.rtf";
};
