// logwrapper.cpp
#include "logwrapper.h"
#include "logdistributor.h"
#include <QCoreApplication>
#include <QThread>
#include <iostream>
#include "logcategories.h"
LogWrapper* LogWrapper::m_instance = nullptr;

LogWrapper* LogWrapper::instance()
{
    if (!m_instance) {
        m_instance = new LogWrapper();
        m_instance->moveToThread(qApp->thread());
    }
    return m_instance;
}

LogWrapper::LogWrapper(QObject *parent) : QObject(parent)
{
    // Убедимся, что объект в основном потоке
    if (QThread::currentThread() != qApp->thread()) {
        moveToThread(qApp->thread());
    }

    // Инициализируем конфигурацию и распределитель
    LogConfig::instance().initDefaultConfig();
    LogDistributor::instance().init();

    // Подключаем сигнал от распределителя к нашему сигналу (для обратной совместимости)
    connect(&LogDistributor::instance(), &LogDistributor::logForUI,
            this, &LogWrapper::logEntryReceived, Qt::QueuedConnection);
}

LogWrapper::~LogWrapper()
{
    m_instance = nullptr;
}

// ==================== ОСНОВНЫЕ МЕТОДЫ ====================

void LogWrapper::log(LogLevel level, const QString& category, const QString& message,
                     const char* file, int line)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    switch (level) {
    case LOG_DEBUG: entry.level = "DEBUG"; break;
    case LOG_INFO: entry.level = "INFO"; break;
    case LOG_WARNING: entry.level = "WARNING"; break;
    case LOG_ERROR: entry.level = "ERROR"; break;
    case LOG_CRITICAL: entry.level = "CRITICAL"; break;
    }
    entry.category = category;
    entry.message = message;
    if (file) {
        entry.sourceFile = QString(file);
        entry.sourceLine = line;
    }
    entry.dataType = DataType::Text;
    log(entry);
}

void LogWrapper::log(const LogEntry& entry)
{
    static thread_local bool inLog = false;
    if (inLog) return;
    inLog = true;
    LogDistributor::instance().distribute(entry);
    inLog = false;
}
// ==================== СТРУКТУРИРОВАННЫЕ ДАННЫЕ ====================

void LogWrapper::logTable(const TableData& table)
{
    LogEntry entry = LogEntry::createTable(LogCategory::UI_DATA, table);
    log(entry);
}

void LogWrapper::logCard(const CardData& card)
{
    LogEntry entry = LogEntry::createCard(LogCategory::UI_STATUS, card);
    log(entry);
}

void LogWrapper::logProgress(const ProgressData& progress)
{
    LogEntry entry = LogEntry::createProgress(LogCategory::UI_STATUS, progress);
    log(entry);
}

// ==================== УСТАРЕВШИЕ МЕТОДЫ ====================
// (для обратной совместимости, пока идёт переход)

void LogWrapper::debug(const QString& message)
{
    log(LOG_DEBUG, LogCategory::GENERAL, message);
}

void LogWrapper::info(const QString& message)
{
    log(LOG_INFO, LogCategory::GENERAL, message);
}

void LogWrapper::warning(const QString& message)
{
    log(LOG_WARNING, LogCategory::GENERAL, message);
}

void LogWrapper::error(const QString& message)
{
    log(LOG_ERROR, LogCategory::GENERAL, message);
}

void LogWrapper::debug(const QString& category, const QString& message)
{
    log(LOG_DEBUG, category, message);
}

void LogWrapper::info(const QString& category, const QString& message)
{
    log(LOG_INFO, category, message);
}

void LogWrapper::warning(const QString& category, const QString& message)
{
    log(LOG_WARNING, category, message);
}

void LogWrapper::error(const QString& category, const QString& message)
{
    log(LOG_ERROR, category, message);
}

void LogWrapper::structuredLog(const QString& level, const QString& category, const QString& message)
{
    LogLevel lvl = LOG_INFO;
    if (level == "DEBUG") lvl = LOG_DEBUG;
    else if (level == "WARNING") lvl = LOG_WARNING;
    else if (level == "ERROR") lvl = LOG_ERROR;
    log(lvl, category, message);
}

void LogWrapper::techLog(const QString& level, const QString& category, const QString& message)
{
    // Технические логи – просто передаём как есть, без дополнительной обработки
    structuredLog(level, category, message);
}

void LogWrapper::emitLogEntry(const LogEntry& entry)
{
    emit logEntryReceived(entry);
}
