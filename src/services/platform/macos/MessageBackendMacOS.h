#pragma once

#include "../../interfaces/IMessageBackend.h"

// macOS displays /Library/Security/PolicyBanner.txt (or .rtf) on the login
// screen before users authenticate - a direct native equivalent of the
// Windows LegalNotice registry values.

class MessageBackendMacOS : public IMessageBackend
{
    Q_OBJECT
public:
    explicit MessageBackendMacOS(QObject* parent = nullptr);

    bool read(StartupMessage& out)        override;
    bool write(const StartupMessage& msg) override;
    bool clear()                          override;

    QString platformDescription() const   override;
    QString lastError() const             override { return m_lastError; }

private:
    QString m_lastError;

    // .txt = macOS 12 and earlier
    static constexpr char kBannerPath[]    = "/Library/Security/PolicyBanner.txt";
    // .rtf = macOS 13+ (Ventura deprecated .txt in favour of .rtf)
    static constexpr char kBannerPathRtf[] = "/Library/Security/PolicyBanner.rtf";
};
