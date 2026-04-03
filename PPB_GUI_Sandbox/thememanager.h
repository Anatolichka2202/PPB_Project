#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum Theme {
        System,
        Dark,
        Light
    };
    Q_ENUM(Theme) // для удобства работы с QML (опционально)

    // Получение единственного экземпляра
    static ThemeManager& instance();

    // Установить тему (System/Dark/Light)
    void setTheme(Theme theme);

    // Получить текущую тему
    Theme currentTheme() const { return m_theme; }

    // Получить CSS для HTML-логов (будет использоваться позже)
    QString getHtmlLogStyle() const;

signals:
    // Сигнал, который испускается при смене темы
    void themeChanged();

private:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager();

    // Применить текущую тему ко всем виджетам через qApp->setStyleSheet
    void applyQss();

    // Определить системную тему (тёмная/светлая)
    Theme detectSystemTheme() const;

    // Загрузить текст из файла ресурса
    QString loadStyleSheet(const QString &resourcePath) const;
private:
    Theme m_theme;
    mutable QString m_cachedHtmlStyle; // кэш стиля для логов
    QStringList getThemeStyleSheets(const QString &themeSubdir) const;
};

#endif // THEMEMANAGER_H
