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

    // Detect the display manager once at startup. This result is used both
    // to select the right message backend and (for the D-Bus path) to pass
    // as a hint to the helper so it doesn't re-run systemctl on every write.
    //
    // isHelperAvailable() is non-blocking — reads the D-Bus daemon's cached
    // activatable services list. Safe to call on the main thread at startup.
    // When the helper is present (.deb / AUR install with PolicyKit),
    // use the D-Bus backend. Otherwise fall back to direct file writes
    // (AppImage installs, or systems without the helper).
    if (MessageBackendLinuxDBus::isHelperAvailable()) {
        // Detect the DM using a temporary MessageBackendLinux instance.
        // platformDescription() triggers the lazy systemctl detection.
        // We then read the result and discard the detector — the D-Bus
        // backend is what the app will actually use.
        MessageBackendLinux detector;
        detector.platformDescription(); // triggers ensureBackendDetected()
        LinuxLoginBackend dm = detector.detectedBackend();
        s.message = new MessageBackendLinuxDBus(dm, parent);
    } else {
        s.message = new MessageBackendLinux(parent);
    }

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
