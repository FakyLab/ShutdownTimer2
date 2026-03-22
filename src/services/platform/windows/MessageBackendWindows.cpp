#include "MessageBackendWindows.h"

MessageBackendWindows::MessageBackendWindows(QObject* parent)
    : IMessageBackend(parent)
{}

QString MessageBackendWindows::platformDescription() const
{
    return QObject::tr("Windows login screen");
}

bool MessageBackendWindows::openKey(HKEY& outKey, REGSAM access)
{
    LONG result = RegCreateKeyExW(
        HKEY_LOCAL_MACHINE,
        kRegPath,
        0, nullptr,
        REG_OPTION_NON_VOLATILE,
        access | KEY_WOW64_64KEY,
        nullptr, &outKey, nullptr
    );

    if (result != ERROR_SUCCESS) {
        m_lastError = QString("RegCreateKeyEx failed: %1").arg(result);
        return false;
    }
    return true;
}

bool MessageBackendWindows::read(StartupMessage& out)
{
    HKEY hKey = nullptr;
    LONG result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        kRegPath,
        0, KEY_READ | KEY_WOW64_64KEY,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        out.title.clear();
        out.body.clear();
        return true;
    }

    auto readValue = [&](const wchar_t* name, QString& dest) {
        DWORD type  = 0;
        DWORD bytes = 0;
        RegQueryValueExW(hKey, name, nullptr, &type, nullptr, &bytes);
        if (bytes == 0) { dest.clear(); return; }

        std::wstring buf(bytes / sizeof(wchar_t) + 1, L'\0');
        RegQueryValueExW(hKey, name, nullptr, &type,
                         reinterpret_cast<LPBYTE>(buf.data()), &bytes);
        dest = QString::fromStdWString(std::wstring(buf.c_str())).trimmed();
    };

    readValue(kRegTitle, out.title);
    readValue(kRegBody,  out.body);

    RegCloseKey(hKey);
    return true;
}

bool MessageBackendWindows::write(const StartupMessage& msg)
{
    HKEY hKey = nullptr;
    if (!openKey(hKey, KEY_SET_VALUE)) return false;

    auto writeValue = [&](const wchar_t* name, const QString& value) -> bool {
        std::wstring ws = value.toStdWString();
        DWORD bytes = static_cast<DWORD>((ws.size() + 1) * sizeof(wchar_t));
        LONG r = RegSetValueExW(
            hKey, name, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(ws.c_str()),
            bytes
        );
        if (r != ERROR_SUCCESS) {
            m_lastError = QString("RegSetValueEx(%1) failed: %2")
                          .arg(QString::fromWCharArray(name)).arg(r);
            return false;
        }
        return true;
    };

    bool ok = writeValue(kRegTitle, msg.title)
           && writeValue(kRegBody,  msg.body);

    RegCloseKey(hKey);
    return ok;
}

bool MessageBackendWindows::clear()
{
    // Write empty strings rather than deleting the values entirely.
    // LegalNoticeCaption/Text may have been set by Group Policy or an IT
    // management tool before ShutdownTimer touched them. Deleting them would
    // destroy that pre-existing configuration. Empty strings suppress the
    // Windows login dialog just as effectively as deletion, and leave the
    // registry in a state that doesn't surprise system administrators.
    StartupMessage empty;
    return write(empty);
}
