// logwrapper.h
#ifndef LOGWRAPPER_H
#define LOGWRAPPER_H

#include <QObject>
#include "logentry.h"
#include "logconfig.h"  // для LogLevel

class LogWrapper : public QObject
{
    Q_OBJECT

public:
    // Удаляем копирование и присваивание
    LogWrapper(const LogWrapper&) = delete;
    LogWrapper& operator=(const LogWrapper&) = delete;

    // Получение экземпляра синглтона
    static LogWrapper* instance();

    // ==================== ОСНОВНЫЕ МЕТОДЫ ЛОГИРОВАНИЯ ====================

    // Простое текстовое логирование с указанием категории
    static void log(LogLevel level, const QString& category, const QString& message,
                    const char* file = nullptr, int line = 0);


    // Прямой метод для отправки готового LogEntry
    static void log(const LogEntry& entry);

    // ==================== СТРУКТУРИРОВАННЫЕ ДАННЫЕ ====================

    static void logTable(const TableData& table);
    static void logCard(const CardData& card);
    static void logProgress(const ProgressData& progress);

    // ==================== УСТАРЕВШИЕ МЕТОДЫ (для обратной совместимости) ====================
    // Они будут удалены после полного перехода на новые макросы.

    static void debug(const QString& message);
    static void info(const QString& message);
    static void warning(const QString& message);
    static void error(const QString& message);

    static void debug(const QString& category, const QString& message);
    static void info(const QString& category, const QString& message);
    static void warning(const QString& category, const QString& message);
    static void error(const QString& category, const QString& message);

    static void structuredLog(const QString& level, const QString& category, const QString& message);
    static void techLog(const QString& level, const QString& category, const QString& message);

signals:
    // Сигнал с полной информацией о логе (для UI)
    void logEntryReceived(const LogEntry& entry);

private slots:
    void emitLogEntry(const LogEntry& entry);  // для обратной совместимости

private:
    explicit LogWrapper(QObject *parent = nullptr);
    ~LogWrapper() override;

    static LogWrapper* m_instance;
};

#endif // LOGWRAPPER_H
