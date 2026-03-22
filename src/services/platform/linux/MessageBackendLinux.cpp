#include "MessageBackendLinux.h"

#include <QFile>
#include <QTemporaryFile>
#include <QTextStream>
#include <QProcess>
#include <QFileInfo>
#include <QDir>

MessageBackendLinux::MessageBackendLinux(QObject* parent)
    : IMessageBackend(parent)
    // DM detection is intentionally deferred - detectBackend() spawns
    // QProcess calls which block the main thread. We detect lazily on
    // first actual use (write/read/clear) instead.
{}

// -- Distro/DM detection --

bool MessageBackendLinux::isServiceActive(const QString& name) const
{
    QProcess proc;
    proc.start("systemctl", QStringList{"is-active", name});
    proc.waitForFinished(3000);
    return proc.readAllStandardOutput().trimmed() == "active";
}

bool MessageBackendLinux::isSlickGreeter() const
{
    // Slick-greeter is identified by its config file or the greeter-session setting
    if (QFile::exists("/etc/lightdm/slick-greeter.conf"))
        return true;

    // Check lightdm.conf for greeter-session=slick-greeter
    QFile conf("/etc/lightdm/lightdm.conf");
    if (conf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(conf.readAll());
        if (content.contains("greeter-session=slick-greeter"))
            return true;
    }
    return false;
}

LinuxLoginBackend MessageBackendLinux::detectBackend() const
{
    if (isServiceActive("gdm") || isServiceActive("gdm3"))
        return LinuxLoginBackend::GDM;
    // PLM (Plasma Login Manager) — KDE Plasma 6.6+, replaces SDDM on Fedora 44+
    // It is a SDDM fork and uses the same config drop-in structure.
    if (isServiceActive("plasma-login-manager"))
        return LinuxLoginBackend::PLM;
    if (isServiceActive("sddm"))
        return LinuxLoginBackend::SDDM;
    if (isServiceActive("lightdm")) {
        // Distinguish slick-greeter (Linux Mint) from GTK greeter (Ubuntu MATE etc.)
        if (isSlickGreeter())
            return LinuxLoginBackend::LightDMSlick;
        return LinuxLoginBackend::LightDM;
    }
    return LinuxLoginBackend::EtcIssue;
}

// Called by write/read/clear/platformDescription on first use.
// Runs systemctl checks once and caches the result.
void MessageBackendLinux::ensureBackendDetected() const
{
    if (!m_backendDetected) {
        m_backend         = detectBackend();
        m_backendDetected = true;
    }
}

QString MessageBackendLinux::platformDescription() const
{
    ensureBackendDetected();
    switch (m_backend) {
        case LinuxLoginBackend::SDDM:
            return tr("login screen (SDDM + /etc/issue)");
        case LinuxLoginBackend::PLM:
            return tr("login screen (Plasma Login Manager + /etc/issue)");
        case LinuxLoginBackend::LightDM:
            return tr("login screen (LightDM GTK greeter + /etc/issue)");
        case LinuxLoginBackend::LightDMSlick:
            return tr("/etc/issue only — slick-greeter (Linux Mint) does not support "
                      "text banners on the graphical login screen");
        case LinuxLoginBackend::GDM:
            return tr("login screen (GDM dconf banner + /etc/issue)");
        default:
            return tr("/etc/issue (TTY login screen)");
    }
}

// -- /etc/issue read/write --
// We own a clearly marked block inside /etc/issue so we don't destroy
// any existing content. On write we insert/replace our block; on clear
// we remove it entirely.

bool MessageBackendLinux::writeEtcIssue(const StartupMessage& msg)
{
    QFile file("/etc/issue");

    // Read existing content, stripping any previous block we wrote
    QString existing;
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_lastError = tr("Cannot read /etc/issue: %1").arg(file.errorString());
            return false;
        }
        QString content = QString::fromUtf8(file.readAll());
        file.close();

        // Remove our previous block if present
        int begin = content.indexOf(kIssueBegin);
        int end   = content.indexOf(kIssueEnd);
        if (begin != -1 && end != -1)
            content.remove(begin, (end - begin) + strlen(kIssueEnd) + 1);
        existing = content.trimmed();
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write /etc/issue: %1").arg(file.errorString());
        return false;
    }

    QTextStream out(&file);
    if (!existing.isEmpty())
        out << existing << "\n\n";

    out << kIssueBegin << "\n";
    if (!msg.title.isEmpty())
        out << msg.title << "\n";
    if (!msg.body.isEmpty())
        out << msg.body << "\n";
    out << kIssueEnd << "\n";
    return true;
}

bool MessageBackendLinux::clearEtcIssue()
{
    QFile file("/etc/issue");
    if (!file.exists()) return true;

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Cannot read /etc/issue: %1").arg(file.errorString());
        return false;
    }
    QString content = QString::fromUtf8(file.readAll());
    file.close();

    int begin = content.indexOf(kIssueBegin);
    int end   = content.indexOf(kIssueEnd);
    if (begin == -1 || end == -1) return true; // nothing to remove

    content.remove(begin, (end - begin) + strlen(kIssueEnd) + 1);
    content = content.trimmed();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write /etc/issue: %1").arg(file.errorString());
        return false;
    }
    QTextStream out(&file);
    if (!content.isEmpty())
        out << content << "\n";
    return true;
}

bool MessageBackendLinux::readEtcIssue(StartupMessage& out)
{
    QFile file("/etc/issue");
    if (!file.exists()) { out.title.clear(); out.body.clear(); return true; }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Cannot read /etc/issue: %1").arg(file.errorString());
        return false;
    }
    QString content = QString::fromUtf8(file.readAll());
    file.close();

    int begin = content.indexOf(kIssueBegin);
    int end   = content.indexOf(kIssueEnd);
    if (begin == -1 || end == -1) { out.title.clear(); out.body.clear(); return true; }

    QString block = content.mid(begin + strlen(kIssueBegin) + 1,
                                 end - begin - strlen(kIssueBegin) - 1).trimmed();
    QStringList lines = block.split('\n');
    out.title = lines.value(0).trimmed();
    out.body  = lines.mid(1).join('\n').trimmed();
    return true;
}

// -- SDDM --
// Write a drop-in config to /etc/sddm.conf.d/ setting the greeter message.
// SDDM's default greeter reads [General] WelcomeMessage.

bool MessageBackendLinux::writeSDDM(const StartupMessage& msg)
{
    QDir().mkpath(QFileInfo(kSddmConf).path());
    QFile file(kSddmConf);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write SDDM config: %1").arg(file.errorString());
        return false;
    }
    QTextStream out(&file);
    out << "# Written by Shutdown Timer — do not edit manually\n";
    out << "[General]\n";
    QString combined = msg.title.isEmpty()
                       ? msg.body
                       : (msg.body.isEmpty() ? msg.title
                                             : msg.title + " — " + msg.body);
    out << "WelcomeMessage=" << combined << "\n";
    return true;
}

bool MessageBackendLinux::clearSDDM()
{
    QFile file(kSddmConf);
    if (file.exists()) {
        if (!file.remove()) {
            m_lastError = tr("Cannot remove SDDM config: %1").arg(file.errorString());
            return false;
        }
    }
    return true;
}

// -- LightDM --
// Write a drop-in to /etc/lightdm/lightdm.conf.d/ setting greeter-session
// message via the GTK greeter's [greeter] section.

bool MessageBackendLinux::writeLightDM(const StartupMessage& msg)
{
    QDir().mkpath(QFileInfo(kLightDMConf).path());
    QFile file(kLightDMConf);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write LightDM config: %1").arg(file.errorString());
        return false;
    }
    QTextStream out(&file);
    out << "# Written by Shutdown Timer — do not edit manually\n";
    out << "[greeter]\n";
    QString combined = msg.title.isEmpty()
                       ? msg.body
                       : (msg.body.isEmpty() ? msg.title
                                             : msg.title + "\n" + msg.body);
    out << "banner-message-enable=true\n";
    out << "banner-message-text=" << combined << "\n";
    return true;
}

bool MessageBackendLinux::clearLightDM()
{
    QFile file(kLightDMConf);
    if (file.exists()) {
        if (!file.remove()) {
            m_lastError = tr("Cannot remove LightDM config: %1").arg(file.errorString());
            return false;
        }
    }
    return true;
}

// -- GDM --
// Ubuntu / GNOME: set login banner via dconf.
// Requires writing a dconf profile and key file, then running dconf update.
// Reference: https://help.gnome.org/admin/system-admin-guide/stable/login-banner.html

bool MessageBackendLinux::writeGDM(const StartupMessage& msg)
{
    // Step 1: Ensure /etc/dconf/profile/gdm exists and has the gdm database
    QDir().mkpath(QFileInfo(kGdmProfile).path());
    QFile profile(kGdmProfile);
    QString profileContent;
    if (profile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        profileContent = QString::fromUtf8(profile.readAll());
        profile.close();
    }
    if (!profileContent.contains("system-db:gdm")) {
        if (!profile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            m_lastError = tr("Cannot write GDM dconf profile: %1").arg(profile.errorString());
            return false;
        }
        QTextStream out(&profile);
        // Preserve existing content, add gdm db entry
        if (!profileContent.trimmed().isEmpty())
            out << profileContent.trimmed() << "\n";
        out << "system-db:gdm\n";
        profile.close();
    }

    // Step 2: Write banner message key file
    QDir().mkpath(kGdmDbDir);
    QFile dbFile(kGdmBannerDb);
    if (!dbFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write GDM banner config: %1").arg(dbFile.errorString());
        return false;
    }
    QString combined = msg.title.isEmpty()
        ? msg.body
        : (msg.body.isEmpty() ? msg.title : msg.title + "\n" + msg.body);
    QTextStream out(&dbFile);
    out << "[org/gnome/login-screen]\n";
    out << "banner-message-enable=true\n";
    out << "banner-message-text='" << combined.replace("'", "\\'") << "'\n";
    dbFile.close();

    // Step 3: Run dconf update to apply changes
    QProcess proc;
    proc.start("dconf", QStringList{"update"});
    proc.waitForFinished(5000);
    if (proc.exitCode() != 0) {
        m_lastError = tr("dconf update failed: %1")
            .arg(QString::fromUtf8(proc.readAllStandardError()).trimmed());
        return false;
    }
    return true;
}

bool MessageBackendLinux::clearGDM()
{
    // Remove the banner key file and run dconf update
    QFile::remove(kGdmBannerDb);

    QProcess proc;
    proc.start("dconf", QStringList{"update"});
    proc.waitForFinished(5000);
    return true;
}

// -- Public interface --

// -- Privilege elevation via pkexec --
//
// The naive approach of re-running the app via pkexec breaks when running
// from an AppImage: pkexec runs the process outside the user's FUSE session,
// so the squashfs mount is inaccessible to the root process.
//
// Fix: write a temporary shell script to /tmp that performs all the file
// operations using standard system tools, then pkexec that script.
// The script needs no Qt, no AppImage, no FUSE — just bash, tee, mkdir, dconf.
// /tmp is always writable by the user and executable by root.

// Shell-safe quoting: wraps value in single quotes, escaping internal ' with '\''
static QString shq(const QString& s)
{
    QString escaped = s;
    escaped.replace("'", "'\\''");
    return "'" + escaped + "'";
}

bool MessageBackendLinux::runWithPkexec(const QStringList& args)
{
    // Check pkexec is available
    QProcess check;
    check.start("which", QStringList{"pkexec"});
    check.waitForFinished(2000);
    if (check.exitCode() != 0) {
        m_lastError = tr("pkexec not found. Please run the app as root to set login messages.");
        return false;
    }

    // Parse the args to determine what operation to perform
    // Supported: --write-message --title T --body B --dm DM
    //            --clear-message
    bool isWrite = (!args.isEmpty() && args[0] == "--write-message");
    bool isClear = (!args.isEmpty() && args[0] == "--clear-message");

    if (!isWrite && !isClear) {
        m_lastError = tr("Unknown pkexec operation.");
        return false;
    }

    // Build a shell script that performs all privileged file operations.
    // This script runs as root via pkexec but needs no AppImage or Qt.
    QStringList script;
    script << "#!/bin/sh";
    script << "set -e";

    if (isWrite) {
        // Parse --title, --body, --dm from args
        QString title, body, dm;
        for (int i = 1; i < args.size(); i++) {
            if (args[i] == "--title") title = args[++i];
            else if (args[i] == "--body")  body  = args[++i];
            else if (args[i] == "--dm")    dm    = args[++i];
        }

        // --- /etc/issue ---
        // Preserve existing content, insert our marked block
        script << "BEGIN_MARKER='# --- ShutdownTimer message begin ---'";
        script << "END_MARKER='# --- ShutdownTimer message end ---'";
        script << "ISSUE_FILE='/etc/issue'";
        script << "if [ -f \"$ISSUE_FILE\" ]; then";
        script << "  EXISTING=$(sed \"/$BEGIN_MARKER/,/$END_MARKER/d\" \"$ISSUE_FILE\" | sed '/^[[:space:]]*$/d')";
        script << "else";
        script << "  EXISTING=''";
        script << "fi";
        script << "{ [ -n \"$EXISTING\" ] && printf '%s\\n\\n' \"$EXISTING\"; "
                  "printf '%s\\n' \"$BEGIN_MARKER\"; "
                  + (title.isEmpty() ? QString() : QString("printf '%s\\n' %1; ").arg(shq(title)))
                  + (body.isEmpty()  ? QString() : QString("printf '%s\\n' %1; ").arg(shq(body)))
                  + "printf '%s\\n' \"$END_MARKER\"; } > \"$ISSUE_FILE\"";

        // --- DM-specific config ---
        if (dm == "sddm") {
            QString combined = title.isEmpty() ? body
                : (body.isEmpty() ? title : title + " \xe2\x80\x94 " + body);
            script << "mkdir -p /etc/sddm.conf.d";
            // Use %s + separate arg so shq(combined) doesn't conflict with
            // the single-quoted format string.
            script << QString("printf '[General]\\nWelcomeMessage=%%s\\n' %1 > "
                              "/etc/sddm.conf.d/shutdown-timer-msg.conf").arg(shq(combined));
        } else if (dm == "lightdm") {
            QString combined = title.isEmpty() ? body
                : (body.isEmpty() ? title : title + "\n" + body);
            script << "mkdir -p /etc/lightdm/lightdm.conf.d";
            // Use %s + separate arg — same reason as SDDM above.
            script << QString("printf '[greeter]\\nbanner-message-enable=true\\n"
                              "banner-message-text=%%s\\n' %1 > "
                              "/etc/lightdm/lightdm.conf.d/shutdown-timer-msg.conf").arg(shq(combined));
        } else if (dm == "gdm") {
            QString combined = title.isEmpty() ? body
                : (body.isEmpty() ? title : title + "\n" + body);
            script << "mkdir -p /etc/dconf/profile /etc/dconf/db/gdm.d";
            script << "if ! grep -q 'system-db:gdm' /etc/dconf/profile/gdm 2>/dev/null; then";
            script << "  printf 'system-db:gdm\\n' >> /etc/dconf/profile/gdm";
            script << "fi";
            // Store the value in a shell variable to avoid nested quoting.
            // dconf key-file values are single-quoted strings.
            // We assign to a variable then substitute into the printf.
            script << QString("DCONF_VAL=%1").arg(shq(combined));
            script << "printf '[org/gnome/login-screen]\\n"
                      "banner-message-enable=true\\n"
                      "banner-message-text=\\'%s\\'\\n' \"$DCONF_VAL\" "
                      "> /etc/dconf/db/gdm.d/01-banner-message";
            script << "dconf update";
        }

    } else { // isClear
        script << "BEGIN_MARKER='# --- ShutdownTimer message begin ---'";
        script << "END_MARKER='# --- ShutdownTimer message end ---'";
        script << "if [ -f /etc/issue ]; then";
        script << "  sed -i \"/$BEGIN_MARKER/,/$END_MARKER/d\" /etc/issue";
        script << "fi";
        script << "rm -f /etc/sddm.conf.d/shutdown-timer-msg.conf";
        script << "rm -f /etc/lightdm/lightdm.conf.d/shutdown-timer-msg.conf";
        script << "rm -f /etc/dconf/db/gdm.d/01-banner-message";
        script << "dconf update 2>/dev/null || true";
    }

    // Write the script to a randomly-named temp file.
    // Random name prevents targeting; owner-only permissions (700) prevent
    // another process running as the same user from reading or replacing it
    // between write and execution — closing the race window.
    QTemporaryFile tmpFile(QDir::tempPath() + "/shutdowntimer_XXXXXX.sh");
    tmpFile.setAutoRemove(false); // we remove it manually after pkexec
    if (!tmpFile.open()) {
        m_lastError = tr("Cannot create temp script: %1").arg(tmpFile.errorString());
        return false;
    }
    QString scriptPath = tmpFile.fileName();
    {
        QTextStream out(&tmpFile);
        for (const QString& line : script)
            out << line << "\n";
        tmpFile.close();
        // chmod 700 — owner read/write/execute only, no access for group or others
        tmpFile.setPermissions(
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }

    QProcess proc;
    proc.start("pkexec", QStringList{"/bin/sh", scriptPath});
    proc.waitForFinished(30000);
    QFile::remove(scriptPath);

    if (proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        m_lastError = err.isEmpty()
            ? tr("Authentication failed or was cancelled.")
            : err;
        return false;
    }
    return true;
}

bool MessageBackendLinux::write(const StartupMessage& msg)
{
    ensureBackendDetected();

    // Try direct write first (succeeds if running as root).
    // For normal users /etc/issue and all DM config paths require root,
    // so both will fail — we elevate via pkexec for the whole operation.
    bool etcOk = writeEtcIssue(msg);
    if (!etcOk) {
        // /etc/issue write failed — elevate the whole operation via pkexec.
        // The headless --write-message handler writes both /etc/issue AND
        // the DM-specific config in one elevated call.
        QString dmType;
        switch (m_backend) {
            case LinuxLoginBackend::SDDM:         dmType = "sddm";    break;
            case LinuxLoginBackend::PLM:          dmType = "sddm";    break;
            case LinuxLoginBackend::LightDM:      dmType = "lightdm"; break;
            case LinuxLoginBackend::GDM:          dmType = "gdm";     break;
            case LinuxLoginBackend::LightDMSlick: dmType = "none";    break;
            default:                              dmType = "none";    break;
        }
        QStringList args;
        args << "--write-message"
             << "--title" << msg.title
             << "--body"  << msg.body
             << "--dm"    << dmType;
        return runWithPkexec(args);
    }

    // /etc/issue write succeeded (running as root or world-writable).
    // Now write the DM-specific config. If that also fails (e.g. /etc/issue
    // was world-writable but /etc/sddm.conf.d/ is not), elevate via pkexec
    // for the full operation — this re-writes /etc/issue as root too, which
    // is harmless and keeps the two writes consistent.
    bool dmOk = true;
    switch (m_backend) {
        case LinuxLoginBackend::SDDM:         dmOk = writeSDDM(msg);    break;
        case LinuxLoginBackend::PLM:          dmOk = writeSDDM(msg);    break;
        case LinuxLoginBackend::LightDM:      dmOk = writeLightDM(msg); break;
        case LinuxLoginBackend::LightDMSlick: dmOk = true;              break;
        case LinuxLoginBackend::GDM:          dmOk = writeGDM(msg);     break;
        default:                              dmOk = true;              break;
    }

    if (!dmOk) {
        // DM config write failed despite /etc/issue succeeding.
        // Elevate the whole operation so both paths are written as root.
        QString dmType;
        switch (m_backend) {
            case LinuxLoginBackend::SDDM:         dmType = "sddm";    break;
            case LinuxLoginBackend::PLM:          dmType = "sddm";    break;
            case LinuxLoginBackend::LightDM:      dmType = "lightdm"; break;
            case LinuxLoginBackend::GDM:          dmType = "gdm";     break;
            default:                              dmType = "none";    break;
        }
        QStringList args;
        args << "--write-message"
             << "--title" << msg.title
             << "--body"  << msg.body
             << "--dm"    << dmType;
        return runWithPkexec(args);
    }

    return true;
}

bool MessageBackendLinux::clear()
{
    ensureBackendDetected();

    bool etcOk = clearEtcIssue();
    if (!etcOk) {
        // /etc/issue clear failed — elevate the whole clear via pkexec.
        // --clear-message removes /etc/issue AND all DM configs as root.
        if (!runWithPkexec(QStringList{"--clear-message"})) return false;
        return true;
    }

    // /etc/issue cleared — now clear the DM config.
    // If that fails, elevate the whole operation (re-clears /etc/issue too,
    // which is harmless).
    bool dmOk = true;
    switch (m_backend) {
        case LinuxLoginBackend::SDDM:         dmOk = clearSDDM();    break;
        case LinuxLoginBackend::PLM:          dmOk = clearSDDM();    break;
        case LinuxLoginBackend::LightDM:      dmOk = clearLightDM(); break;
        case LinuxLoginBackend::LightDMSlick: dmOk = true;           break;
        case LinuxLoginBackend::GDM:          dmOk = clearGDM();     break;
        default:                              dmOk = true;           break;
    }

    if (!dmOk)
        return runWithPkexec(QStringList{"--clear-message"});

    return true;
}

bool MessageBackendLinux::read(StartupMessage& out)
{
    // Read from /etc/issue as the canonical source
    return readEtcIssue(out);
}
