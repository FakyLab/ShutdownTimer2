#pragma once

#include "../../interfaces/IMessageBackend.h"

enum class LinuxLoginBackend {
    EtcIssue,       // universal TTY fallback — always written
    SDDM,           // KDE display manager — clean config file
    PLM,            // Plasma Login Manager (KDE Plasma 6.6+, Fedora 44+) — SDDM fork
    LightDM,        // LightDM with GTK greeter (Ubuntu MATE, Xubuntu)
    LightDMSlick,   // LightDM with slick-greeter (Linux Mint) — no text banner
    GDM,            // GNOME — uses dconf for login banner
    Unknown
};

class MessageBackendLinux : public IMessageBackend
{
    Q_OBJECT
public:
    explicit MessageBackendLinux(QObject* parent = nullptr);

    bool read(StartupMessage& out)        override;
    bool write(const StartupMessage& msg) override;
    bool clear()                          override;

    QString platformDescription() const   override;
    QString lastError() const             override { return m_lastError; }

    // Exposed for UI status reporting
    LinuxLoginBackend detectedBackend() const { return m_backend; }

private:
    LinuxLoginBackend detectBackend() const;
    void ensureBackendDetected() const;  // lazy one-time DM detection
    bool isServiceActive(const QString& name) const;
    bool runWithPkexec(const QStringList& args);  // elevate via pkexec

    bool writeEtcIssue(const StartupMessage& msg);
    bool clearEtcIssue();
    bool readEtcIssue(StartupMessage& out);

    bool writeSDDM(const StartupMessage& msg);
    bool clearSDDM();

    bool writeLightDM(const StartupMessage& msg);
    bool clearLightDM();

    bool writeGDM(const StartupMessage& msg);
    bool clearGDM();

    bool isSlickGreeter() const;

    // Lazily detected on first write/read - not at construction time
    // to avoid spawning QProcess calls on the main thread at startup.
    mutable LinuxLoginBackend m_backend     = LinuxLoginBackend::Unknown;
    mutable bool              m_backendDetected = false;
    QString                   m_lastError;

    // Sentinel comments used to identify our block in /etc/issue
    static constexpr char kIssueBegin[] = "# --- ShutdownTimer message begin ---";
    static constexpr char kIssueEnd[]   = "# --- ShutdownTimer message end ---";

    // SDDM config drop-in path
    static constexpr char kSddmConf[]    = "/etc/sddm.conf.d/shutdown-timer-msg.conf";
    // PLM uses the same drop-in directory as SDDM (it is a SDDM fork)
    static constexpr char kPlmConf[]     = "/etc/sddm.conf.d/shutdown-timer-msg.conf";

    // LightDM greeter config drop-in
    static constexpr char kLightDMConf[] = "/etc/lightdm/lightdm.conf.d/shutdown-timer-msg.conf";

    // GDM dconf paths for login banner
    static constexpr char kGdmProfile[]  = "/etc/dconf/profile/gdm";
    static constexpr char kGdmDbDir[]    = "/etc/dconf/db/gdm.d";
    static constexpr char kGdmBannerDb[] = "/etc/dconf/db/gdm.d/01-banner-message";
};
