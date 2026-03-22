#include "AppSettingsModel.h"
#include <QSettings>

AppSettingsModel::AppSettingsModel(QObject* parent)
    : QObject(parent)
{
    load();
}

void AppSettingsModel::load()
{
    QSettings s(kOrg, kApp);
    m_language = LanguageManager::fromCode(s.value(kLang, "en").toString());
    m_geometry = s.value(kGeo).toByteArray();
}

void AppSettingsModel::save()
{
    QSettings s(kOrg, kApp);
    s.setValue(kLang, LanguageManager::toCode(m_language));
    if (!m_geometry.isEmpty())
        s.setValue(kGeo, m_geometry);
}

void AppSettingsModel::setLanguage(AppLanguage lang)
{
    m_language = lang;
    save();
}

void AppSettingsModel::setWindowGeometry(const QByteArray& geo)
{
    m_geometry = geo;
    save();
}
