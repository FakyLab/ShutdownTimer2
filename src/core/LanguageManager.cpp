#include "LanguageManager.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QProcessEnvironment>

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
{}

void LanguageManager::applyLanguage(AppLanguage lang)
{
    m_current = lang;

    qApp->removeTranslator(&m_translator);

    QString code    = toCode(lang);
    QString relName = QString("i18n/app_%1.qm").arg(code);

    // Search order - first match wins:
    //
    // 1. Current working directory (development / build-dir run)
    // 2. Next to the executable (Windows installed, Linux portable)
    // 3. /usr/share/shutdowntimer/ (Linux .deb installed)
    // 4. Inside the .app bundle Resources/ (macOS)
    //
    // This covers every deployment scenario across all three platforms
    // without compile-time platform guards in the language logic.

    // Check if an explicit i18n directory was set by the AppRun wrapper
    // (used when running from an AppImage where the mount path is dynamic)
    QString envI18n = QProcessEnvironment::systemEnvironment()
                      .value("SHUTDOWNTIMER_I18N_DIR");

    QStringList candidates;

    // Environment override first - AppImage sets this via AppRun
    if (!envI18n.isEmpty())
        candidates << envI18n + "/app_" + code + ".qm";

    candidates << QDir::currentPath() + "/" + relName
               << QCoreApplication::applicationDirPath() + "/" + relName
               << "/usr/share/shutdowntimer/" + relName;

#if defined(Q_OS_MACOS)
    // macOS .app bundle: <AppName>.app/Contents/Resources/i18n/
    candidates << QCoreApplication::applicationDirPath() + "/../Resources/" + relName;
#endif

    QString loaded;
    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            loaded = path;
            break;
        }
    }

    if (!loaded.isEmpty() && m_translator.load(loaded))
        qApp->installTranslator(&m_translator);

    if (lang == AppLanguage::Arabic)
        qApp->setLayoutDirection(Qt::RightToLeft);
    else
        qApp->setLayoutDirection(Qt::LeftToRight);

    emit languageChanged(lang);
}

QString LanguageManager::languageDisplayName(AppLanguage lang)
{
    switch (lang) {
        case AppLanguage::English:           return QStringLiteral("English");
        case AppLanguage::Arabic:            return QStringLiteral("العربية");
        case AppLanguage::Korean:            return QStringLiteral("한국어");
        case AppLanguage::Spanish:           return QStringLiteral("Español");
        case AppLanguage::French:            return QStringLiteral("Français");
        case AppLanguage::German:            return QStringLiteral("Deutsch");
        case AppLanguage::PortugueseBR:      return QStringLiteral("Português (Brasil)");
        case AppLanguage::ChineseSimplified: return QStringLiteral("中文（简体）");
        case AppLanguage::Japanese:          return QStringLiteral("日本語");
    }
    return {};
}

AppLanguage LanguageManager::fromCode(const QString& code)
{
    if (code == "ar")    return AppLanguage::Arabic;
    if (code == "ko")    return AppLanguage::Korean;
    if (code == "es")    return AppLanguage::Spanish;
    if (code == "fr")    return AppLanguage::French;
    if (code == "de")    return AppLanguage::German;
    if (code == "pt_BR") return AppLanguage::PortugueseBR;
    if (code == "zh_CN") return AppLanguage::ChineseSimplified;
    if (code == "ja")    return AppLanguage::Japanese;
    return AppLanguage::English;
}

QString LanguageManager::toCode(AppLanguage lang)
{
    switch (lang) {
        case AppLanguage::Arabic:            return QStringLiteral("ar");
        case AppLanguage::Korean:            return QStringLiteral("ko");
        case AppLanguage::Spanish:           return QStringLiteral("es");
        case AppLanguage::French:            return QStringLiteral("fr");
        case AppLanguage::German:            return QStringLiteral("de");
        case AppLanguage::PortugueseBR:      return QStringLiteral("pt_BR");
        case AppLanguage::ChineseSimplified: return QStringLiteral("zh_CN");
        case AppLanguage::Japanese:          return QStringLiteral("ja");
        default:                             return QStringLiteral("en");
    }
}
