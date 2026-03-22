#pragma once

// MessageBackendLinuxDBus
//
// Proxies all login message operations through the privileged D-Bus system
// service org.fakylab.ShutdownTimerHelper via PolicyKit-protected methods.
// Used when running in a Flatpak sandbox or when the helper is installed.
//
// Selected at runtime by PlatformServiceFactory::create() via
// isHelperAvailable() — falls back to MessageBackendLinux when the helper
// is not present (plain .deb / AppImage installs).

#include "../../interfaces/IMessageBackend.h"
#include <QDBusInterface>
#include <QDBusReply>
#include <QString>

class MessageBackendLinuxDBus : public IMessageBackend
{
    Q_OBJECT
public:
    explicit MessageBackendLinuxDBus(QObject* parent = nullptr);

    bool read(StartupMessage& out)        override;
    bool write(const StartupMessage& msg) override;
    bool clear()                          override;

    QString platformDescription() const   override;
    QString lastError() const             override { return m_lastError; }

    // Returns true if the helper service is registered or activatable on
    // the system bus. Non-blocking — uses activatableServiceNames() which
    // reads a cached list rather than making a round-trip call.
    static bool isHelperAvailable();

private:
    QDBusInterface* iface() const;

    mutable QDBusInterface* m_iface     = nullptr;
    mutable QString         m_lastError;

    // static constexpr const char* avoids multiple-definition linker errors
    // when this header is included from more than one translation unit.
    static constexpr const char* kService   = "org.fakylab.ShutdownTimerHelper";
    static constexpr const char* kPath      = "/org/fakylab/ShutdownTimerHelper";
    static constexpr const char* kInterface = "org.fakylab.ShutdownTimerHelper";
};
