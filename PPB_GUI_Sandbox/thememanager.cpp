#include "thememanager.h"
#include <QApplication>
#include <QFile>
#include <QDebug>
#include <QGuiApplication>
#include <QStyleHints>

ThemeManager& ThemeManager::instance()
{
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_theme(System)
{
    // Определяем системную тему и применяем её
    m_theme = detectSystemTheme();
    applyQss();
}

ThemeManager::~ThemeManager()
{
}


void ThemeManager::setTheme(Theme theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    applyQss();
    emit themeChanged(); // сигнал для обновления логов и других виджетов
}

void ThemeManager::applyQss()
{

    QString baseStyle = loadStyleSheet(":/themes/resources/base.qss");
    qDebug() << "baseStyle loaded, size:" << baseStyle.size();

    QString themeStyle;
    switch (m_theme) {
    case Dark:
        themeStyle = loadStyleSheet(":/themes/resources/dark.qss");
        break;
    case Light:
        themeStyle = loadStyleSheet(":/themes/resources/light.qss");
        break;
    case System:
    default:
        if (detectSystemTheme() == Dark)
            themeStyle = loadStyleSheet(":/themes/resources/dark.qss");
        else
            themeStyle = loadStyleSheet(":/themes/resources/light.qss");
        break;
    }

    QString fullStyle = baseStyle + "\n" + themeStyle;
    qDebug() << "fullStyle size:" << fullStyle.size();
    qApp->setStyleSheet(fullStyle);

    // Для логов тоже с префиксом
    m_cachedHtmlStyle = loadStyleSheet(":/themes/resources/logstyles.css");
}

QString ThemeManager::getHtmlLogStyle() const
{
    return m_cachedHtmlStyle;
}

ThemeManager::Theme ThemeManager::detectSystemTheme() const
{
    // Начиная с Qt 6.5, есть встроенный метод
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    auto scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark)
        return Dark;
    else
        return Light;
#else
    // определяем по умолчанию светлую тему
    return Light;
#endif
}

QString ThemeManager::loadStyleSheet(const QString &resourcePath) const
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open style sheet:" << resourcePath;
        return QString();
    }
    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();
    return content;
}
