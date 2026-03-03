// logdistributor.h
#ifndef LOGDISTRIBUTOR_H
#define LOGDISTRIBUTOR_H

#include <QObject>
#include <QMap>
#include <map>
#include <memory>
#include "logentry.h"
#include <QFile>
// Forward declarations
class LogHandler;
class FileHandler;
class ConsoleHandler;
class UiSignalHandler;

class LogDistributor : public QObject
{
    Q_OBJECT
public:
    static LogDistributor& instance();

    void init(); // создаёт обработчики на основе LogConfig
    void distribute(const LogEntry& entry);

signals:
    void logForUI(const LogEntry& entry); // для совместимости

private:
    LogDistributor(QObject* parent = nullptr);
    ~LogDistributor();

    std::map<QString, std::unique_ptr<LogHandler>> m_handlers;
};

// Абстрактный обработчик
class LogHandler {
public:
    virtual ~LogHandler() = default;
    virtual void handle(const LogEntry& entry) = 0;
};

// Файловый обработчик
class FileHandler : public LogHandler {
public:
    FileHandler(const QString& fileNamePattern, qint64 maxSize, int maxBackups);
    ~FileHandler();
    void handle(const LogEntry& entry) override;
private:
    void rotateIfNeeded();
    void openNewFile();
    QString m_baseName;
    qint64 m_maxSize;
    int m_maxBackups;
    QFile m_file;
    QDateTime m_startTime;
};

// Консольный обработчик
class ConsoleHandler : public LogHandler {
public:
    void handle(const LogEntry& entry) override;
};

// Обработчик для UI (отдельный QObject, чтобы иметь сигнал)
class UiSignalHandler : public QObject, public LogHandler {
    Q_OBJECT
public:
    explicit UiSignalHandler(LogDistributor* distributor);
    void handle(const LogEntry& entry) override;
signals:
    void logEntryReceived(const LogEntry& entry);
private:
    LogDistributor* m_distributor;
};

#endif // LOGDISTRIBUTOR_H
