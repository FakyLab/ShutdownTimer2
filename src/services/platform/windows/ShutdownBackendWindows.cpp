#include "ShutdownBackendWindows.h"

ShutdownBackendWindows::ShutdownBackendWindows(QObject* parent)
    : IShutdownBackend(parent)
{}

bool ShutdownBackendWindows::isHibernateAvailable()
{
    SYSTEM_POWER_CAPABILITIES caps{};
    if (!GetPwrCapabilities(&caps))
        return false;
    return caps.HiberFilePresent && caps.SystemS4;
}

bool ShutdownBackendWindows::isSleepAvailable()
{
    SYSTEM_POWER_CAPABILITIES caps{};
    if (!GetPwrCapabilities(&caps))
        return false;
    return caps.SystemS3 != FALSE;
}

bool ShutdownBackendWindows::acquireShutdownPrivilege()
{
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &hToken))
    {
        m_lastError = QString("OpenProcessToken failed: %1").arg(GetLastError());
        return false;
    }

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME,
                               &tp.Privileges[0].Luid))
    {
        m_lastError = QString("LookupPrivilegeValue failed: %1").arg(GetLastError());
        CloseHandle(hToken);
        return false;
    }

    BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    DWORD adjustErr = GetLastError();
    CloseHandle(hToken);

    if (!ok || adjustErr == ERROR_NOT_ALL_ASSIGNED) {
        m_lastError = QString("AdjustTokenPrivileges failed: %1").arg(adjustErr);
        return false;
    }
    return true;
}

bool ShutdownBackendWindows::scheduleShutdown(ShutdownAction action, int seconds, bool force)
{
    if (action == ShutdownAction::Hibernate || action == ShutdownAction::Sleep) {
        BOOL hibernate = (action == ShutdownAction::Hibernate) ? TRUE : FALSE;
        BOOL ok = SetSuspendState(hibernate, FALSE, FALSE);
        if (!ok) {
            DWORD err = GetLastError();
            m_lastError = QString("SetSuspendState(%1) failed: %2")
                          .arg(hibernate ? "hibernate" : "sleep")
                          .arg(err);
            emit errorOccurred(m_lastError);
            return false;
        }
        m_pending = false;
        return true;
    }

    if (!acquireShutdownPrivilege()) {
        emit errorOccurred(m_lastError);
        return false;
    }

    const DWORD reason = SHTDN_REASON_MAJOR_HARDWARE |
                         SHTDN_REASON_MINOR_MAINTENANCE |
                         SHTDN_REASON_FLAG_PLANNED;

    BOOL restart = (action == ShutdownAction::Restart) ? TRUE : FALSE;

    BOOL ok = InitiateSystemShutdownExW(
        nullptr,
        nullptr,
        static_cast<DWORD>(seconds),
        force ? TRUE : FALSE,
        restart,
        reason
    );

    if (!ok) {
        DWORD err = GetLastError();
        m_lastError = QString("InitiateSystemShutdownEx failed: %1").arg(err);
        emit errorOccurred(m_lastError);
        return false;
    }

    m_pending = true;
    return true;
}

bool ShutdownBackendWindows::cancelShutdown()
{
    if (!acquireShutdownPrivilege()) {
        emit errorOccurred(m_lastError);
        return false;
    }

    AbortSystemShutdown(nullptr);
    m_pending = false;
    return true;
}
