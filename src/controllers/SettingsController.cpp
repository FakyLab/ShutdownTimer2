#include "SettingsController.h"
#include <QMainWindow>

SettingsController::SettingsController(AppSettingsModel* model,
                                       LanguageManager*  langMgr,
                                       QObject*          parent)
    : QObject(parent)
    , m_model(model)
    , m_langMgr(langMgr)
{
    connect(m_langMgr, &LanguageManager::languageChanged,
            this,      &SettingsController::languageApplied);
}

AppLanguage SettingsController::currentLanguage() const
{
    return m_model->language();
}

void SettingsController::restoreWindowGeometry(QMainWindow* window) const
{
    const QByteArray& geo = m_model->windowGeometry();
    if (!geo.isEmpty())
        window->restoreGeometry(geo);
}

void SettingsController::onLanguageChanged(AppLanguage lang)
{
    m_model->setLanguage(lang);
    m_langMgr->applyLanguage(lang);
}

void SettingsController::onWindowGeometryChanged(const QByteArray& geo)
{
    m_model->setWindowGeometry(geo);
}
