#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QApplication>

enum class AppLanguage {
    English,
    Arabic,
    Korean,
    Spanish,
    French,
    German,
    PortugueseBR,
    ChineseSimplified,
    Japanese
};

class LanguageManager : public QObject
{
    Q_OBJECT
public:
    explicit LanguageManager(QObject* parent = nullptr);

    void applyLanguage(AppLanguage lang);
    AppLanguage currentLanguage() const { return m_current; }

    // Returns true if current language is RTL
    bool isRTL() const { return m_current == AppLanguage::Arabic; }
    bool isCJK() const { return m_current == AppLanguage::ChineseSimplified || m_current == AppLanguage::Japanese; }

    static QString languageDisplayName(AppLanguage lang);
    static AppLanguage fromCode(const QString& code);
    static QString toCode(AppLanguage lang);

signals:
    void languageChanged(AppLanguage lang);

private:
    AppLanguage  m_current = AppLanguage::English;
    QTranslator  m_translator;
};
