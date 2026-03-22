#include "MessageBackendLinux.h"

#include <QFile>
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

LinuxLoginBackend MessageBackendLinux::detectBackend() const
{
    if (isServiceActive("gdm") || isServiceActive("gdm3"))
        return LinuxLoginBackend::GDM;
    if (isServiceActive("sddm"))
        return LinuxLoginBackend::SDDM;
    if (isServiceActive("lightdm"))
        return LinuxLoginBackend::LightDM;
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
            return tr("login screen (SDDM config + /etc/issue)");
        case LinuxLoginBackend::LightDM:
            return tr("login screen (LightDM config + /etc/issue)");
        case LinuxLoginBackend::GDM:
            return tr("/etc/issue only (GDM has no native login banner support)");
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

// -- Public interface --

bool MessageBackendLinux::write(const StartupMessage& msg)
{
    ensureBackendDetected();
    // Always write /etc/issue as universal fallback
    if (!writeEtcIssue(msg)) return false;

    // Additionally write to the detected graphical DM
    switch (m_backend) {
        case LinuxLoginBackend::SDDM:    return writeSDDM(msg);
        case LinuxLoginBackend::LightDM: return writeLightDM(msg);
        case LinuxLoginBackend::GDM:
            // GDM has no native banner - /etc/issue already written above
            return true;
        default:
            return true;
    }
}

bool MessageBackendLinux::clear()
{
    ensureBackendDetected();
    bool ok = clearEtcIssue();

    switch (m_backend) {
        case LinuxLoginBackend::SDDM:    ok &= clearSDDM();    break;
        case LinuxLoginBackend::LightDM: ok &= clearLightDM(); break;
        default: break;
    }
    return ok;
}

bool MessageBackendLinux::read(StartupMessage& out)
{
    // Read from /etc/issue as the canonical source
    return readEtcIssue(out);
}
