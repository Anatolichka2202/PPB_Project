// logdistributor.cpp
#include "logdistributor.h"
#include "logconfig.h"
#include "logcategories.h"
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>

static LogLevel levelFromString(const QString& level) {
    if (level == "DEBUG") return LOG_DEBUG;
    if (level == "INFO") return LOG_INFO;
    if (level == "WARNING") return LOG_WARNING;
    if (level == "ERROR") return LOG_ERROR;
    if (level == "CRITICAL") return LOG_CRITICAL;
    return LOG_INFO;
}

LogDistributor& LogDistributor::instance()
{
    static LogDistributor instance;
    return instance;
}

LogDistributor::LogDistributor(QObject* parent) : QObject(parent) {}

LogDistributor::~LogDistributor() = default;

void LogDistributor::init()
{
    auto& config = LogConfig::instance();
    QDir dir("logs");
    if (!dir.exists()) dir.mkpath(".");

    for (const QString& id : config.getChannelIds()) {
        auto cfg = config.getChannel(id);
        if (!cfg.enabled) continue;

        if (id == "ui") {
            auto handler = std::make_unique<UiSignalHandler>(this);
            connect(handler.get(), &UiSignalHandler::logEntryReceived,
                    this, &LogDistributor::logForUI);
            m_handlers[id] = std::move(handler);
        }
        else if (id == "console") {
            m_handlers[id] = std::make_unique<ConsoleHandler>();
        }
        else if (id.endsWith("_file")) {
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
            QString fileName = QString("logs/%1_%2.log").arg(id).arg(timestamp);
            m_handlers[id] = std::make_unique<FileHandler>(fileName, cfg.maxFileSize, cfg.maxBackupFiles);
        }
    }
}

void LogDistributor::distribute(const LogEntry& entry)
{
    auto& config = LogConfig::instance();
    for (auto& [channelId, handler] : m_handlers) {
        if (config.shouldLogToChannel(channelId, levelFromString(entry.level), entry.category)) {
            handler->handle(entry);
        }
    }
}

// ========== FileHandler ==========
FileHandler::FileHandler(const QString& fileName, qint64 maxSize, int maxBackups)
    : m_baseName(fileName), m_maxSize(maxSize), m_maxBackups(maxBackups)
{
    m_file.setFileName(m_baseName);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        qWarning() << "Cannot open log file:" << m_baseName;
    }
}

FileHandler::~FileHandler()
{
    if (m_file.isOpen()) m_file.close();
}

void FileHandler::handle(const LogEntry& entry)
{
    if (!m_file.isOpen()) return;
    QTextStream stream(&m_file);
    // Для tech_file и all_file используем JSON, для oper_file - toString()
    if (m_baseName.contains("tech_") || m_baseName.contains("all_")) {
        stream << entry.toJson() << "\n";
    } else {
        stream << entry.toString() << "\n";
    }
    stream.flush();
    // TODO: реализовать ротацию по размеру
}

void FileHandler::openNewFile()
{
    // TODO: реализация ротации
}

void FileHandler::rotateIfNeeded()
{
    // TODO: проверка размера и ротация
}

// ========== ConsoleHandler ==========
void ConsoleHandler::handle(const LogEntry& entry)
{
    qDebug().noquote() << entry.toLegacyFormat();
}

// ========== UiSignalHandler ==========
UiSignalHandler::UiSignalHandler(LogDistributor* distributor)
    : QObject(distributor), m_distributor(distributor) {}

void UiSignalHandler::handle(const LogEntry& entry)
{
    emit logEntryReceived(entry);
}
