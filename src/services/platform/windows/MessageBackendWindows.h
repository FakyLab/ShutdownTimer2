#pragma once

#include "../../interfaces/IMessageBackend.h"
#include <windows.h>

class MessageBackendWindows : public IMessageBackend
{
    Q_OBJECT
public:
    explicit MessageBackendWindows(QObject* parent = nullptr);

    bool read(StartupMessage& out)        override;
    bool write(const StartupMessage& msg) override;
    bool clear()                          override;

    QString platformDescription() const   override;
    QString lastError() const             override { return m_lastError; }

private:
    bool openKey(HKEY& outKey, REGSAM access);

    QString m_lastError;

    static constexpr wchar_t kRegPath[]  =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
    static constexpr wchar_t kRegTitle[] = L"LegalNoticeCaption";
    static constexpr wchar_t kRegBody[]  = L"LegalNoticeText";
};
