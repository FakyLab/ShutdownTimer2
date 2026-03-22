#include "PlatformServiceFactory.h"

#include <QtGlobal>
#include <QDebug>

#if defined(Q_OS_WIN)
#  include "platform/windows/ShutdownBackendWindows.h"
#  include "platform/windows/MessageBackendWindows.h"
#  include "platform/windows/AutoClearBackendWindows.h"
#elif defined(Q_OS_LINUX)
#  include "platform/linux/ShutdownBackendLinux.h"
#  include "platform/linux/MessageBackendLinux.h"
#  include "platform/linux/MessageBackendLinuxDBus.h"
#  include "platform/linux/AutoClearBackendLinux.h"
#elif defined(Q_OS_MACOS)
#  include "platform/macos/ShutdownBackendMacOS.h"
#  include "platform/macos/MessageBackendMacOS.h"
#  include "platform/macos/AutoClearBackendMacOS.h"
#else
#  error "Unsupported platform — no backend implementation available."
#endif

PlatformServices PlatformServiceFactory::create(QObject* parent)
{
    PlatformServices s;

#if defined(Q_OS_WIN)
    s.shutdown  = new ShutdownBackendWindows(parent);
    s.message   = new MessageBackendWindows(parent);
    s.autoClear = new AutoClearBackendWindows(parent);

#elif defined(Q_OS_LINUX)
    s.shutdown  = new ShutdownBackendLinux(parent);
    s.autoClear = new AutoClearBackendLinux(parent);

    // isHelperAvailable() is non-blocking — reads the D-Bus daemon's cached
    // activatable services list. Safe to call on the main thread at startup.
    // When the helper is present (Flatpak / system install with PolicyKit),
    // use the D-Bus backend. Otherwise fall back to direct file writes.
    if (MessageBackendLinuxDBus::isHelperAvailable())
        s.message = new MessageBackendLinuxDBus(parent);
    else
        s.message = new MessageBackendLinux(parent);

#elif defined(Q_OS_MACOS)
    s.shutdown  = new ShutdownBackendMacOS(parent);
    s.message   = new MessageBackendMacOS(parent);
    s.autoClear = new AutoClearBackendMacOS(parent);
#endif

    // Query hardware capabilities once at startup — may involve system calls.
    // Caching here avoids repeated blocking calls from the UI layer.
    Q_ASSERT_X(s.shutdown != nullptr, "PlatformServiceFactory",
               "Shutdown backend is null — unsupported platform?");

    s.hibernateAvailable = s.shutdown->isHibernateAvailable();
    s.sleepAvailable     = s.shutdown->isSleepAvailable();

    return s;
}
