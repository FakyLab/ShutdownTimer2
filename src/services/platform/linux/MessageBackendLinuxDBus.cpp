#include "MessageBackendLinuxDBus.h"
#include "MessageBackendLinux.h"   // for LinuxLoginBackend enum
#include "SlickNotification.h"     // post-login notification for Linux Mint

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

MessageBackendLinuxDBus::MessageBackendLinuxDBus(LinuxLoginBackend dm, QObject* parent)
    : IMessageBackend(parent)
    , m_dm(dm)
{}

// Non-blocking check — reads the activatable services list cached by dbus-daemon.
// Does NOT make a round-trip call, so it's safe to call on the main thread at startup.
bool MessageBackendLinuxDBus::isHelperAvailable()
{
    QDBusConnectionInterface* bus = QDBusConnection::systemBus().interface();
    if (!bus) return false;

    // Check if already running
    if (bus->isServiceRegistered(QLatin1String("org.fakylab.ShutdownTimerHelper")))
        return true;

    // Check if activatable (auto-start configured but not yet running)
    QDBusReply<QStringList> activatable = bus->activatableServiceNames();
    if (activatable.isValid())
        return activatable.value().contains(QLatin1String("org.fakylab.ShutdownTimerHelper"));

    return false;
}

// -- IMessageBackend interface ------------------------------------------------

bool MessageBackendLinuxDBus::write(const StartupMessage& msg)
{
    // Resolve the DM type from the detected backend so the helper doesn't
    // have to re-run systemctl detection on every write call.
    QString dmHint;
    switch (m_dm) {
        case LinuxLoginBackend::SDDM:         dmHint = "sddm";    break;
        case LinuxLoginBackend::PLM:          dmHint = "sddm";    break;
        case LinuxLoginBackend::LightDM:      dmHint = "lightdm"; break;
        case LinuxLoginBackend::LightDMSlick: dmHint = "none";    break;
        case LinuxLoginBackend::GDM:          dmHint = "gdm";     break;
        default:                              dmHint = "auto";    break;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(
        QLatin1String(kService),
        QLatin1String(kPath),
        QLatin1String(kInterface),
        QLatin1String("WriteMessage")
    );
    call << msg.title << msg.body << dmHint;

    QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        m_lastError = reply.errorMessage();
        return false;
    }

    // For slick-greeter (Linux Mint): also schedule a post-login desktop
    // notification since the helper only writes /etc/issue (no DM config).
    // This file is user-owned — no root needed, not handled by the helper.
    if (m_dm == LinuxLoginBackend::LightDMSlick)
        SlickNotification::write(msg);

    return true;
}

bool MessageBackendLinuxDBus::clear()
{
    // Always clean up any pending Slick notification first — even if the
    // D-Bus clear fails, we don't want a stale notification at next login.
    if (m_dm == LinuxLoginBackend::LightDMSlick)
        SlickNotification::clear();

    QDBusMessage call = QDBusMessage::createMethodCall(
        QLatin1String(kService),
        QLatin1String(kPath),
        QLatin1String(kInterface),
        QLatin1String("ClearMessage")
    );

    QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        m_lastError = reply.errorMessage();
        return false;
    }
    return true;
}

bool MessageBackendLinuxDBus::read(StartupMessage& out)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QLatin1String(kService),
        QLatin1String(kPath),
        QLatin1String(kInterface),
        QLatin1String("ReadMessage")
    );

    QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        m_lastError = reply.errorMessage();
        return false;
    }

    // ReadMessage returns (ss) — two strings, title and body.
    // reply.arguments() contains them as separate QVariant items.
    QList<QVariant> args = reply.arguments();
    out.title = args.value(0).toString();
    out.body  = args.value(1).toString();
    return true;
}

QString MessageBackendLinuxDBus::platformDescription() const
{
    return tr("login screen via D-Bus helper (Flatpak / sandboxed)");
}
