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
    , m_theme(Dark)
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

QStringList ThemeManager::getThemeStyleSheets(const QString &themeSubdir) const
{
    QStringList sheets;
    // Порядок важен: сначала общий base.qss, потом theme.qss, потом специфичные
    sheets << loadStyleSheet(":/base.qss");
    sheets << loadStyleSheet(QString(":/%1/theme.qss").arg(themeSubdir));
    sheets << loadStyleSheet(QString(":/%1/connection.qss").arg(themeSubdir));
    sheets << loadStyleSheet(QString(":/%1/controll.qss").arg(themeSubdir));
    sheets << loadStyleSheet(QString(":/%1/gratten.qss").arg(themeSubdir));
    sheets << loadStyleSheet(QString(":/%1/status.qss").arg(themeSubdir));
    sheets << loadStyleSheet(QString(":/%1/enoth.qss").arg(themeSubdir));
    return sheets;
}

void ThemeManager::applyQss()
{
    QString baseStyle = loadStyleSheet(":/base.qss");
    QString themeSubdir = (m_theme == Dark) ? "dark" : "light";
    QString themeStyle = loadStyleSheet(QString(":/%1/theme.qss").arg(themeSubdir));
    QString connectionStyle = loadStyleSheet(QString(":/%1/connection.qss").arg(themeSubdir));
    QString controllStyle = loadStyleSheet(QString(":/%1/controll.qss").arg(themeSubdir));
    QString grattenStyle = loadStyleSheet(QString(":/%1/gratten.qss").arg(themeSubdir));
    QString statusStyle = loadStyleSheet(QString(":/%1/status.qss").arg(themeSubdir));
    QString enothStyle = loadStyleSheet(QString(":/%1/enoth.qss").arg(themeSubdir));

    QString fullStyle = baseStyle + "\n" + themeStyle + "\n" +
                        connectionStyle + "\n" + controllStyle + "\n" +
                        grattenStyle + "\n" + statusStyle + "\n" + enothStyle;
    qApp->setStyleSheet(fullStyle);

    // Для логов
    QString logStylePath = (m_theme == Dark) ? ":/logstyle_dark.css" : ":/logstyle_light.css";
    m_cachedHtmlStyle = loadStyleSheet(logStylePath);
    emit themeChanged();
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
