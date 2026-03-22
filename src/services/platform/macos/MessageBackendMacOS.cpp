#include "MessageBackendMacOS.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

MessageBackendMacOS::MessageBackendMacOS(QObject* parent)
    : IMessageBackend(parent)
{}

QString MessageBackendMacOS::platformDescription() const
{
    return tr("macOS login screen");
}

// -- runElevated --
// Runs a shell command as root via osascript.
// Shows a single native macOS password dialog for the entire command.
// Caller is responsible for shell-escaping all values in shellCmd.

bool MessageBackendMacOS::runElevated(const QString& shellCmd)
{
    // Write the AppleScript to a temp file and run `osascript <file>`.
    // shellCmd is embedded inside an AppleScript double-quoted string.
    // Only \ and " are special inside AppleScript double-quoted strings.
    // shellCmd is built using shellArgQuote() which double-quotes argument
    // values and escapes only \ and " — so no conflict between escaping layers.
    QString appleCmd = shellCmd;
    // These are the only two chars special in AppleScript double-quoted strings.
    // shellArgQuote() already escapes these in argument values, but the
    // structural characters (spaces, &&, ;) pass through unchanged — correct.
    // No further processing needed: appleCmd is already AppleScript-safe.

    const QString scriptPath = "/tmp/shutdowntimer_elevate.applescript";
    {
        QFile f(scriptPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            m_lastError = tr("Cannot write temp script: %1").arg(f.errorString());
            return false;
        }
        QTextStream out(&f);
        out << "do shell script \"" << appleCmd << "\" with administrator privileges\n";
    }

    QProcess proc;
    proc.start("osascript", QStringList{scriptPath});
    proc.waitForFinished(30000);
    QFile::remove(scriptPath);

    if (proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        m_lastError = err.isEmpty()
            ? tr("Authentication failed or was cancelled.")
            : err;
        return false;
    }
    return true;
}

// -- shellArgQuote --
// Wraps a value in double quotes for safe embedding in a shell command,
// escaping only \ and " — the two characters special inside double-quoted
// shell strings. This is also safe for embedding in an AppleScript
// double-quoted string (which has the same two special characters),
// so no additional escaping layer is needed in runElevated().
static QString shellArgQuote(const QString& s)
{
    QString escaped = s;
    escaped.replace("\\", "\\\\");  // \ → \\
    escaped.replace("\"", "\\\"");  // " → \"
    return "\"" + escaped + "\"";
}

// -- write --
//
// Writes the message using two mechanisms in a single elevated call:
//
//   1. `defaults write` to loginwindow LoginwindowText  (primary)
//      Apple's intended API. Shows as subtitle under the machine name
//      on the login screen. Works on all macOS versions incl. Sequoia.
//      No chmod needed — defaults handles permissions itself.
//
//   2. PolicyBanner .txt + .rtf files                   (secondary)
//      Legacy/compliance full-screen banner the user must dismiss.
//      Needs chmod 644 so loginwindow (different user) can read them.
//
// Everything happens in one osascript call — one password prompt.

bool MessageBackendMacOS::write(const StartupMessage& msg)
{
    // Combined string for loginwindow (title — body, or whichever is set).
    // QChar(0x2014) is Unicode em dash U+2014. Using the codepoint directly
    // avoids the C++ hex-escape-as-Latin-1 pitfall (\xe2\x80\x94 would store
    // three separate Latin-1 chars, not the em dash U+2014).
    static const QString kSep = QString(" ") + QChar(0x2014) + QString(" ");
    QString combined;
    if (!msg.title.isEmpty() && !msg.body.isEmpty())
        combined = msg.title + kSep + msg.body;
    else if (!msg.title.isEmpty())
        combined = msg.title;
    else
        combined = msg.body;

    // .txt content for PolicyBanner (macOS 12-)
    QString txtContent;
    if (!msg.title.isEmpty()) txtContent += msg.title + "\n\n";
    if (!msg.body.isEmpty())  txtContent += msg.body  + "\n";

    // .rtf content for PolicyBanner (macOS 13+)
    // RTF requires escaping: \ { }
    QString rtfBody = msg.body;
    rtfBody.replace("\\", "\\\\").replace("{", "\\{").replace("}", "\\}");
    QString rtfTitle = msg.title;
    rtfTitle.replace("\\", "\\\\").replace("{", "\\{").replace("}", "\\}");
    QString rtfContent = "{\\rtf1\\ansi\\deff0\n";
    if (!rtfTitle.isEmpty()) rtfContent += "{\\b " + rtfTitle + "}\\line\\line\n";
    if (!rtfBody.isEmpty())  rtfContent += rtfBody + "\n";
    rtfContent += "}\n";

    // -- Try direct write first --
    // Works when running as root (CI, sudo). For normal users this fails
    // with EPERM and we fall through to the elevated path below.
    {
        QProcess defProc;
        defProc.start("defaults", QStringList{
            "write", kLoginwindowPlist, kLoginwindowKey, combined
        });
        defProc.waitForFinished(5000);

        if (defProc.exitCode() == 0) {
            // Direct defaults write succeeded — write banner files directly too.
            auto writeAndChmod = [](const QString& path, const QString& content) {
                QDir().mkpath(QFileInfo(path).path());
                QFile f(path);
                if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                    QTextStream out(&f);
                    out << content;
                    f.close();
                    // chmod 644: owner rw, group r, other r
                    f.setPermissions(
                        QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                        QFileDevice::ReadGroup | QFileDevice::ReadOther);
                }
            };
            writeAndChmod(kBannerPath,    txtContent);
            writeAndChmod(kBannerPathRtf, rtfContent);
            return true;
        }
    }

    // -- Elevated path --
    // Write banner content to /tmp first (no root needed), then use a single
    // osascript call to: set loginwindow pref + cp + chmod both banner files.
    // /tmp is accessible by the system user that osascript elevates to.

    const QString tmpTxt = "/tmp/shutdowntimer_banner.txt";
    const QString tmpRtf = "/tmp/shutdowntimer_banner.rtf";

    auto writeTmp = [&](const QString& path, const QString& content) -> bool {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            m_lastError = tr("Cannot write temp file %1: %2").arg(path, f.errorString());
            return false;
        }
        QTextStream out(&f);
        out << content;
        return true;
    };

    if (!writeTmp(tmpTxt, txtContent)) return false;
    if (!writeTmp(tmpRtf, rtfContent)) return false;

    // Single compound shell command — one password prompt covers everything:
    //   1. defaults write  (loginwindow primary)
    //   2. mkdir -p + cp + chmod 644 for .txt  (PolicyBanner legacy)
    //   3. cp + chmod 644 for .rtf             (PolicyBanner Ventura+)
    //   4. rm temp files
    QString shellCmd =
        "defaults write " + shellArgQuote(kLoginwindowPlist)
        + " " + shellArgQuote(kLoginwindowKey)
        + " " + shellArgQuote(combined)
        + " && mkdir -p /Library/Security"
        + " && cp "  + shellArgQuote(tmpTxt) + " " + shellArgQuote(kBannerPath)
        + " && chmod 644 " + shellArgQuote(kBannerPath)
        + " && cp "  + shellArgQuote(tmpRtf) + " " + shellArgQuote(kBannerPathRtf)
        + " && chmod 644 " + shellArgQuote(kBannerPathRtf)
        + " && rm -f " + shellArgQuote(tmpTxt) + " " + shellArgQuote(tmpRtf);

    bool ok = runElevated(shellCmd);

    // Clean up temp files regardless of outcome (runElevated rm may have failed)
    QFile::remove(tmpTxt);
    QFile::remove(tmpRtf);

    return ok;
}

// -- read --
//
// loginwindow LoginwindowText is the canonical source.
// Falls back to PolicyBanner.txt for messages set by older app versions
// or external tools (MDM, manual admin).

bool MessageBackendMacOS::read(StartupMessage& out)
{
    QProcess proc;
    proc.start("defaults", QStringList{
        "read", kLoginwindowPlist, kLoginwindowKey
    });
    proc.waitForFinished(5000);

    if (proc.exitCode() == 0) {
        QString value = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!value.isEmpty()) {
            // Recover title/body split using the em dash separator this app writes.
            // If absent treat the whole value as body (set externally by MDM etc.).
            static const QString kSep = QString(" ") + QChar(0x2014) + QString(" ");
            int pos = value.indexOf(kSep);
            if (pos != -1) {
                out.title = value.left(pos).trimmed();
                out.body  = value.mid(pos + kSep.length()).trimmed();
            } else {
                out.title.clear();
                out.body = value;
            }
            return true;
        }
    }

    // Fall back: parse PolicyBanner.txt
    QFile file(kBannerPath);
    if (!file.exists()) {
        out.title.clear();
        out.body.clear();
        return true;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Cannot read PolicyBanner.txt: %1").arg(file.errorString());
        return false;
    }

    QString content = QString::fromUtf8(file.readAll()).trimmed();
    QStringList lines = content.split('\n');
    out.title = lines.value(0).trimmed();
    int bodyStart = 1;
    while (bodyStart < lines.size() && lines[bodyStart].trimmed().isEmpty())
        bodyStart++;
    out.body = lines.mid(bodyStart).join('\n').trimmed();
    return true;
}

// -- clear --
//
// Removes loginwindow preference and both PolicyBanner files.
// Tries direct removal first; falls back to a single elevated call.

bool MessageBackendMacOS::clear()
{
    // Try direct removal first (works if running as root)
    QProcess defProc;
    defProc.start("defaults", QStringList{
        "delete", kLoginwindowPlist, kLoginwindowKey
    });
    defProc.waitForFinished(5000);
    // exit code 1 = key didn't exist — not an error

    QFile fileTxt(kBannerPath);
    QFile fileRtf(kBannerPathRtf);
    bool txtGone = !fileTxt.exists() || fileTxt.remove();
    bool rtfGone = !fileRtf.exists() || fileRtf.remove();

    if (txtGone && rtfGone)
        return true;

    // Banner files couldn't be removed — elevate.
    // Re-issue the defaults delete too in case it also failed silently.
    // Use ; between commands so rm runs even if defaults delete fails
    // (the key may already be gone from the direct attempt above).
    QString shellCmd =
        "defaults delete " + shellArgQuote(kLoginwindowPlist)
        + " " + shellArgQuote(kLoginwindowKey)
        + " ; rm -f "
        + shellArgQuote(kBannerPath) + " "
        + shellArgQuote(kBannerPathRtf);

    return runElevated(shellCmd);
}
