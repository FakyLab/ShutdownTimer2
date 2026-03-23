#include "AutoClearBackendMacOS.h"
#include "MessageBackendMacOS.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>

AutoClearBackendMacOS::AutoClearBackendMacOS(QObject* parent)
    : IAutoClearBackend(parent)
{}

// -- Sentinel path ------------------------------------------------------------
//
// Lives next to message.json so both are always in the same directory.
// The --show-notification handler reads this to decide whether to delete
// the message after showing it (auto-clear) or leave it (persistent).

QString AutoClearBackendMacOS::sentinelPath()
{
    // Strip "message.json" from the message path and append "autoclear"
    QString msgPath = MessageBackendMacOS::messageFilePath();
    return QFileInfo(msgPath).dir().filePath("autoclear");
}

// -- IAutoClearBackend --------------------------------------------------------

bool AutoClearBackendMacOS::schedule()
{
    // Ensure the parent directory exists (same dir as message.json)
    QString path = sentinelPath();
    QDir().mkpath(QFileInfo(path).path());

    // Write an empty sentinel file — existence is all that matters
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastError = tr("Cannot write auto-clear sentinel: %1").arg(f.errorString());
        return false;
    }
    f.close();
    return true;
}

bool AutoClearBackendMacOS::cancel()
{
    QFile::remove(sentinelPath());
    return true;
}

bool AutoClearBackendMacOS::exists()
{
    return QFile::exists(sentinelPath());
}
