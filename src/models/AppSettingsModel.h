#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include "../core/LanguageManager.h"

class AppSettingsModel : public QObject
{
    Q_OBJECT
public:
    explicit AppSettingsModel(QObject* parent = nullptr);

    AppLanguage language()       const { return m_language; }
    QByteArray  windowGeometry() const { return m_geometry; }

    void setLanguage(AppLanguage lang);
    void setWindowGeometry(const QByteArray& geo);

    void load();
    void save();

private:
    AppLanguage m_language = AppLanguage::English;
    QByteArray  m_geometry;

    static constexpr char kOrg[]  = "ShutdownTimer";
    static constexpr char kApp[]  = "ShutdownTimer";
    static constexpr char kLang[] = "language";
    static constexpr char kGeo[]  = "windowGeometry";
};
