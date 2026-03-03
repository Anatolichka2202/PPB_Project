// logconfig.cpp
#include "logconfig.h"
#include "logcategories.h"
#include "logmacros.h"  // для новых макросов (пока они еще не созданы, но будут)
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>

LogConfig& LogConfig::instance()
{
    static LogConfig instance;
    return instance;
}

LogConfig::LogConfig(QObject* parent) : QObject(parent)
{
    createDefaultChannels();
}

LogConfig::~LogConfig()
{
}

void LogConfig::createDefaultChannels()
{
    QMutexLocker locker(&m_mutex);

    // 1. UI канал
    LogChannelConfig uiConfig;
    uiConfig.name = "UI Channel";
    uiConfig.description = "Вывод в пользовательский интерфейс";
    uiConfig.minLevel = LOG_INFO;
    uiConfig.enabled = true;
    uiConfig.includeCategories = {
        LogCategory::UI_STATUS,
        LogCategory::UI_OPERATION,
        LogCategory::UI_RESULT,
        LogCategory::UI_ALERT,
        LogCategory::UI_DATA,
        LogCategory::UI_PPB_RESULT,
        LogCategory::TECH_STATE,
        LogCategory::SYSTEM,
        LogCategory::GENERAL
    };
    m_channels["ui"] = uiConfig;

    // 2. Технический файл
    LogChannelConfig techFileConfig;
    techFileConfig.name = "Technical File";
    techFileConfig.description = "Полный технический лог для отладки";
    techFileConfig.minLevel = LOG_DEBUG;
    techFileConfig.enabled = true;
    techFileConfig.includeCategories = {
        LogCategory::TECH_DEBUG,
        LogCategory::TECH_NETWORK,
        LogCategory::TECH_PROTOCOL,
        LogCategory::TECH_STATE,
        LogCategory::TECH_THREAD,
        LogCategory::TECH_PERFORMANCE,
        LogCategory::SYSTEM,
        LogCategory::GENERAL
    };
    techFileConfig.maxFileSize = 50 * 1024 * 1024;
    techFileConfig.maxBackupFiles = 10;
    m_channels["tech_file"] = techFileConfig;

    // 3. Операционный файл
    LogChannelConfig operFileConfig;
    operFileConfig.name = "Operational File";
    operFileConfig.description = "Операционный лог для отчётности";
    operFileConfig.minLevel = LOG_INFO;
    operFileConfig.enabled = true;
    operFileConfig.includeCategories = {
        LogCategory::OP_SESSION,
        LogCategory::OP_OPERATION,
        LogCategory::OP_MEASUREMENT,
        LogCategory::OP_SUMMARY,
        LogCategory::UI_OPERATION,
        LogCategory::UI_RESULT,
        LogCategory::UI_ALERT,
        LogCategory::UI_PPB_RESULT,
        LogCategory::SYSTEM,
        LogCategory::GENERAL
    };
    operFileConfig.maxFileSize = 20 * 1024 * 1024;
    operFileConfig.maxBackupFiles = 5;
    m_channels["oper_file"] = operFileConfig;

    // 4. Общий файл (все сообщения)
    LogChannelConfig allFileConfig;
    allFileConfig.name = "All Messages File";
    allFileConfig.description = "Все сообщения всех категорий";
    allFileConfig.minLevel = LOG_DEBUG;
    allFileConfig.enabled = true;
    // includeCategories пусто = все разрешены
    allFileConfig.maxFileSize = 100 * 1024 * 1024;
    allFileConfig.maxBackupFiles = 20;
    m_channels["all_file"] = allFileConfig;

    // 5. Консоль
    LogChannelConfig consoleConfig;
    consoleConfig.name = "Console";
    consoleConfig.description = "Вывод в консоль";
    consoleConfig.minLevel = LOG_DEBUG;
    consoleConfig.enabled = true;
    // все категории
    m_channels["console"] = consoleConfig;
}

void LogConfig::initDefaultConfig()
{
    // Уже создано в конструкторе
}

void LogConfig::addChannel(const QString& id, const LogChannelConfig& config)
{
    QMutexLocker locker(&m_mutex);

    if (m_channels.contains(id)) {
        LOG_WARN(LogCategory::CONFIG, QString("Канал с ID '%1' уже существует").arg(id));
        return;
    }

    m_channels[id] = config;
    emit channelAdded(id);

    LOG_DEBUG(LogCategory::CONFIG, QString("Добавлен канал логирования: %1").arg(config.name));
}

void LogConfig::removeChannel(const QString& id)
{
    QMutexLocker locker(&m_mutex);

    if (m_channels.remove(id)) {
        emit channelRemoved(id);
        LOG_DEBUG(LogCategory::CONFIG, QString("Удалён канал логирования: %1").arg(id));
    }
}

void LogConfig::enableChannel(const QString& id, bool enabled)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_channels.find(id);
    if (it != m_channels.end()) {
        it->enabled = enabled;
        emit configChanged(id);

        LOG_DEBUG(LogCategory::CONFIG,
                  QString("Канал '%1' %2").arg(id).arg(enabled ? "включён" : "выключен"));
    }
}

LogChannelConfig LogConfig::getChannel(const QString& id) const
{
    QMutexLocker locker(&m_mutex);
    return m_channels.value(id);
}

QList<QString> LogConfig::getChannelIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_channels.keys();
}

bool LogConfig::shouldLogToChannel(const QString& channelId,
                                   LogLevel level,
                                   const QString& category) const
{
    QMutexLocker locker(&m_mutex);

    auto it = m_channels.constFind(channelId);
    if (it == m_channels.constEnd()) {
        return false;
    }

    const LogChannelConfig& config = it.value();

    if (!config.enabled) {
        return false;
    }

    if (level < config.minLevel) {
        return false;
    }

    return config.isCategoryAllowed(category);
}

bool LogConfig::saveToFile(const QString& filename)
{
    QMutexLocker locker(&m_mutex);

    QJsonObject root;
    QJsonObject channelsObj;

    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        const QString& id = it.key();
        const LogChannelConfig& config = it.value();

        QJsonObject channelObj;
        channelObj["name"] = config.name;
        channelObj["description"] = config.description;
        channelObj["enabled"] = config.enabled;
        channelObj["minLevel"] = static_cast<int>(config.minLevel);
        channelObj["showTimestamp"] = config.showTimestamp;
        channelObj["showCategory"] = config.showCategory;
        channelObj["showLevel"] = config.showLevel;
        channelObj["showThreadInfo"] = config.showThreadInfo;
        channelObj["maxFileSize"] = static_cast<qint64>(config.maxFileSize);
        channelObj["maxBackupFiles"] = config.maxBackupFiles;

        QJsonArray includeArray;
        for (const QString& cat : config.includeCategories) {
            includeArray.append(cat);
        }
        channelObj["includeCategories"] = includeArray;

        QJsonArray excludeArray;
        for (const QString& cat : config.excludeCategories) {
            excludeArray.append(cat);
        }
        channelObj["excludeCategories"] = excludeArray;

        channelsObj[id] = channelObj;
    }

    root["channels"] = channelsObj;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(LogCategory::CONFIG,
                  QString("Не удалось открыть файл для записи: %1").arg(filename));
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson());
    file.close();

    LOG_INFO(LogCategory::CONFIG,
             QString("Конфигурация логирования сохранена в: %1").arg(filename));
    return true;
}

bool LogConfig::loadFromFile(const QString& filename)
{
    QMutexLocker locker(&m_mutex);

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_WARN(LogCategory::CONFIG,
                 QString("Не удалось открыть файл для чтения: %1").arg(filename));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        LOG_ERROR(LogCategory::CONFIG,
                  QString("Неверный формат JSON в файле: %1").arg(filename));
        return false;
    }

    QJsonObject root = doc.object();
    QJsonObject channelsObj = root["channels"].toObject();

    // Очищаем существующие каналы (кроме стандартных)
    QStringList keys = m_channels.keys();
    for (const QString& key : keys) {
        if (!key.startsWith("default_")) {
            m_channels.remove(key);
        }
    }

    // Загружаем каналы из файла
    for (auto it = channelsObj.begin(); it != channelsObj.end(); ++it) {
        QString id = it.key();
        QJsonObject channelObj = it.value().toObject();

        LogChannelConfig config;
        config.name = channelObj["name"].toString();
        config.description = channelObj["description"].toString();
        config.enabled = channelObj["enabled"].toBool(true);
        config.minLevel = static_cast<LogLevel>(channelObj["minLevel"].toInt(LOG_INFO));
        config.showTimestamp = channelObj["showTimestamp"].toBool(true);
        config.showCategory = channelObj["showCategory"].toBool(true);
        config.showLevel = channelObj["showLevel"].toBool(true);
        config.showThreadInfo = channelObj["showThreadInfo"].toBool(false);
        config.maxFileSize = static_cast<qint64>(channelObj["maxFileSize"].toDouble(10 * 1024 * 1024));
        config.maxBackupFiles = channelObj["maxBackupFiles"].toInt(5);

        QJsonArray includeArray = channelObj["includeCategories"].toArray();
        for (const QJsonValue& val : includeArray) {
            config.includeCategories.insert(val.toString());
        }

        QJsonArray excludeArray = channelObj["excludeCategories"].toArray();
        for (const QJsonValue& val : excludeArray) {
            config.excludeCategories.insert(val.toString());
        }

        m_channels[id] = config;
    }

    LOG_INFO(LogCategory::CONFIG,
             QString("Конфигурация логирования загружена из: %1").arg(filename));
    return true;
}

void LogConfig::setMinLevel(const QString& channelId, LogLevel level)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_channels.find(channelId);
    if (it != m_channels.end()) {
        it->minLevel = level;
        emit configChanged(channelId);

        LOG_DEBUG(LogCategory::CONFIG,
                  QString("Установлен уровень %1 для канала '%2'").arg(level).arg(channelId));
    }
}
