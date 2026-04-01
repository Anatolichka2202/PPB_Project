#include "logdistributor.h"
#include "logconfig.h"
#include "logcategories.h"
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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
    : QObject(nullptr)
    , m_baseName(fileName), m_maxSize(maxSize), m_maxBackups(maxBackups)
{
    openNewFile();
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(100); // сброс каждые 100 мс
    connect(m_flushTimer, &QTimer::timeout, this, &FileHandler::flush);
    m_flushTimer->start();
}

FileHandler::~FileHandler()
{
    flush();
    m_file.close();
}

void FileHandler::handle(const LogEntry& entry)
{
    if (!m_file.isOpen()) return;

    QString line;
    // Для tech_file – JSON с отступами, для oper_file – текстовый
    if (m_baseName.contains("tech_")) {
        // Преобразуем компактный JSON в отформатированный
        QByteArray jsonData = entry.toJson().toUtf8();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (!doc.isNull()) {
            line = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        } else {
            // fallback: если не распарсилось, выводим как есть
            line = QString::fromUtf8(jsonData);
        }
    } else {
        line = entry.toLegacyFormat();
    }
    m_buffer.append(line.toUtf8());
    m_lineCount++;

    // Если буфер превысил 1 МБ, сбрасываем
    if (m_buffer.size() > 1024 * 1024) {
        flush();
    }
}

void FileHandler::flush()
{
    if (m_buffer.isEmpty()) return;
    m_file.write(m_buffer);
    m_buffer.clear();
    m_file.flush();
    rotateIfNeeded();
    m_lineCount = 0;
}

void FileHandler::openNewFile()
{
    m_file.setFileName(m_baseName);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        qWarning() << "Cannot open log file:" << m_baseName;
    }
}

void FileHandler::rotateIfNeeded()
{
    if (m_file.size() < m_maxSize) return;
    m_file.close();

    // Переименовываем существующие бэкапы
    for (int i = m_maxBackups; i > 0; --i) {
        QString oldName = QString("%1.%2").arg(m_baseName).arg(i);
        QString newName = QString("%1.%2").arg(m_baseName).arg(i+1);
        QFile::rename(oldName, newName);
    }
    QFile::rename(m_baseName, m_baseName + ".1");
    openNewFile();
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
