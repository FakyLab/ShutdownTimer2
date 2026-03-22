#include "MessageBackendMacOS.h"

#include <QFile>
#include <QTextStream>
#include <QDir>

MessageBackendMacOS::MessageBackendMacOS(QObject* parent)
    : IMessageBackend(parent)
{}

QString MessageBackendMacOS::platformDescription() const
{
    return tr("macOS login screen (PolicyBanner)");
}

bool MessageBackendMacOS::write(const StartupMessage& msg)
{
    QDir().mkpath(QFileInfo(kBannerPath).path());

    // Write .txt for macOS 12 and earlier
    QFile fileTxt(kBannerPath);
    if (!fileTxt.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write PolicyBanner.txt: %1").arg(fileTxt.errorString());
        return false;
    }
    QTextStream outTxt(&fileTxt);
    if (!msg.title.isEmpty())
        outTxt << msg.title << "\n\n";
    if (!msg.body.isEmpty())
        outTxt << msg.body << "\n";
    fileTxt.close();

    // Write .rtf for macOS 13+ (Ventura and later deprecated .txt)
    // Minimal valid RTF wrapping the plain text content.
    QFile fileRtf(kBannerPathRtf);
    if (fileRtf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream outRtf(&fileRtf);
        QString body = msg.body;
        body.replace("\\", "\\\\").replace("{", "\\{").replace("}", "\\}");
        QString title = msg.title;
        title.replace("\\", "\\\\").replace("{", "\\{").replace("}", "\\}");

        outRtf << "{\\rtf1\\ansi\\deff0\n";
        if (!title.isEmpty())
            outRtf << "{\\b " << title << "}\\line\\line\n";
        if (!body.isEmpty())
            outRtf << body << "\n";
        outRtf << "}\n";
        fileRtf.close();
    }
    // .rtf write failure is non-fatal — .txt is still written

    return true;
}

bool MessageBackendMacOS::read(StartupMessage& out)
{
    QFile file(kBannerPath);
    if (!file.exists()) { out.title.clear(); out.body.clear(); return true; }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Cannot read PolicyBanner.txt: %1").arg(file.errorString());
        return false;
    }

    QString content = QString::fromUtf8(file.readAll()).trimmed();
    // First non-empty line = title, remainder = body
    QStringList lines = content.split('\n');
    out.title = lines.value(0).trimmed();
    // Skip the blank separator line after title
    int bodyStart = 1;
    while (bodyStart < lines.size() && lines[bodyStart].trimmed().isEmpty())
        bodyStart++;
    out.body = lines.mid(bodyStart).join('\n').trimmed();
    return true;
}

bool MessageBackendMacOS::clear()
{
    // Remove both .txt and .rtf variants
    QFile fileTxt(kBannerPath);
    if (fileTxt.exists() && !fileTxt.remove()) {
        m_lastError = tr("Cannot remove PolicyBanner.txt: %1").arg(fileTxt.errorString());
        return false;
    }
    QFile fileRtf(kBannerPathRtf);
    if (fileRtf.exists())
        fileRtf.remove();  // non-fatal if .rtf removal fails
    return true;
}
