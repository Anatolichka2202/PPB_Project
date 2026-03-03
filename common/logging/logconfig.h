// logconfig.h
#ifndef LOGCONFIG_H
#define LOGCONFIG_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QMap>
#include <QMutex>

// Уровни логирования
enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3,
    LOG_CRITICAL = 4
};

// Конфигурация канала вывода
struct LogChannelConfig {
    QString name;
    QString description;

    // Настройки вывода
    bool enabled = true;
    LogLevel minLevel = LOG_INFO;

    // Форматы вывода (пока не используются в обработчиках, но можно оставить для будущего)
    bool showTimestamp = true;
    bool showCategory = true;
    bool showLevel = true;
    bool showThreadInfo = false;

    // Максимальный размер (для файлов)
    qint64 maxFileSize = 10 * 1024 * 1024; // 10 MB
    int maxBackupFiles = 5;

    // Фильтрация по категориям
    QSet<QString> includeCategories;   // Если пусто - все разрешены
    QSet<QString> excludeCategories;   // Приоритет над include

    bool isCategoryAllowed(const QString& category) const {
        if (excludeCategories.contains(category))
            return false;
        if (!includeCategories.isEmpty() && !includeCategories.contains(category))
            return false;
        return true;
    }
};

// Главный класс конфигурации логирования
class LogConfig : public QObject
{
    Q_OBJECT

public:
    static LogConfig& instance();

    // Инициализация стандартной конфигурации
    void initDefaultConfig();

    // Управление каналами
    void addChannel(const QString& id, const LogChannelConfig& config);
    void removeChannel(const QString& id);
    void enableChannel(const QString& id, bool enabled);

    // Получение конфигурации
    LogChannelConfig getChannel(const QString& id) const;
    QList<QString> getChannelIds() const;

    // Проверка, должен ли лог попасть в канал
    bool shouldLogToChannel(const QString& channelId,
                            LogLevel level,
                            const QString& category) const;

    // Сохранение/загрузка конфигурации
    bool saveToFile(const QString& filename);
    bool loadFromFile(const QString& filename);

    // Динамическое изменение настроек
    void setMinLevel(const QString& channelId, LogLevel level);

signals:
    void configChanged(const QString& channelId);
    void channelAdded(const QString& channelId);
    void channelRemoved(const QString& channelId);

private:
    explicit LogConfig(QObject* parent = nullptr);
    ~LogConfig();

    void createDefaultChannels();

    QMap<QString, LogChannelConfig> m_channels;
    mutable QRecursiveMutex m_mutex;
};

#endif // LOGCONFIG_H
