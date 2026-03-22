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
    // Linux: no privileged operations needed for messages.
    // MessageBackendLinux writes to ~/.config/ — fully unprivileged.
    s.shutdown  = new ShutdownBackendLinux(parent);
    s.message   = new MessageBackendLinux(parent);
    s.autoClear = new AutoClearBackendLinux(parent);

#elif defined(Q_OS_MACOS)
    s.shutdown  = new ShutdownBackendMacOS(parent);
    s.message   = new MessageBackendMacOS(parent);
    s.autoClear = new AutoClearBackendMacOS(parent);
#endif

    Q_ASSERT_X(s.shutdown != nullptr, "PlatformServiceFactory",
               "Shutdown backend is null — unsupported platform?");

    s.hibernateAvailable = s.shutdown->isHibernateAvailable();
    s.sleepAvailable     = s.shutdown->isSleepAvailable();

    return s;
}
