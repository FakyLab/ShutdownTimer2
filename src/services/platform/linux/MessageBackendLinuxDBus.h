#pragma once

// MessageBackendLinuxDBus
//
// Proxies all login message operations through the privileged D-Bus system
// service org.fakylab.ShutdownTimerHelper via PolicyKit-protected methods.
// Used when the helper is installed (.deb / AUR / Flatpak).
//
// Selected at runtime by PlatformServiceFactory::create() via
// isHelperAvailable() — falls back to MessageBackendLinux when the helper
// is not present (AppImage installs).

#include "../../interfaces/IMessageBackend.h"
#include <QDBusReply>
#include <QString>

// Forward-declare the enum so we can store the DM hint without a full include.
// The full header is included in the .cpp where LinuxLoginBackend is used.
enum class LinuxLoginBackend;

class MessageBackendLinuxDBus : public IMessageBackend
{
    Q_OBJECT
public:
    // dm: the display manager detected by MessageBackendLinux at startup.
    // Passed to the helper as a hint so it doesn't re-run systemctl checks.
    explicit MessageBackendLinuxDBus(LinuxLoginBackend dm,
                                     QObject* parent = nullptr);

    bool read(StartupMessage& out)        override;
    bool write(const StartupMessage& msg) override;
    bool clear()                          override;

    QString platformDescription() const   override;
    QString lastError() const             override { return m_lastError; }

    // Returns true if the helper service is registered or activatable on
    // the system bus. Non-blocking — reads the cached activatable services list.
    static bool isHelperAvailable();

private:
    LinuxLoginBackend m_dm;
    mutable QString   m_lastError;

    static constexpr const char* kService   = "org.fakylab.ShutdownTimerHelper";
    static constexpr const char* kPath      = "/org/fakylab/ShutdownTimerHelper";
    static constexpr const char* kInterface = "org.fakylab.ShutdownTimerHelper";
};
