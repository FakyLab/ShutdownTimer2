#include "MessageBackendLinuxDBus.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

MessageBackendLinuxDBus::MessageBackendLinuxDBus(QObject* parent)
    : IMessageBackend(parent)
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
    QDBusMessage call = QDBusMessage::createMethodCall(
        QLatin1String(kService),
        QLatin1String(kPath),
        QLatin1String(kInterface),
        QLatin1String("WriteMessage")
    );
    call << msg.title << msg.body;

    QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        m_lastError = reply.errorMessage();
        return false;
    }
    return true;
}

bool MessageBackendLinuxDBus::clear()
{
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
