#pragma once

#include <QObject>
#include "../models/AppSettingsModel.h"
#include "../core/LanguageManager.h"

class QMainWindow;

class SettingsController : public QObject
{
    Q_OBJECT
public:
    explicit SettingsController(AppSettingsModel* model,
                                LanguageManager*  langMgr,
                                QObject*          parent = nullptr);

    AppLanguage currentLanguage() const;
    void        restoreWindowGeometry(QMainWindow* window) const;

public slots:
    void onLanguageChanged(AppLanguage lang);
    void onWindowGeometryChanged(const QByteArray& geo);

signals:
    void languageApplied(AppLanguage lang);

private:
    AppSettingsModel* m_model;
    LanguageManager*  m_langMgr;
};
