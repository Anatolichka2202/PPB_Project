// logdistributor.h
#ifndef LOGDISTRIBUTOR_H
#define LOGDISTRIBUTOR_H

#include <QObject>
#include <QMap>
#include <map>
#include <memory>
#include "logentry.h"
#include <QFile>
#include <QTimer>

// Forward declarations
class LogHandler;
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

// Файловый обработчик – теперь QObject, чтобы иметь таймер и слоты
class FileHandler : public QObject, public LogHandler {
    Q_OBJECT
public:
    FileHandler(const QString& fileNamePattern, qint64 maxSize, int maxBackups);
    ~FileHandler();
    void handle(const LogEntry& entry) override;

public slots:
    void flush();  // слот для таймера

private:
    void rotateIfNeeded();
    void openNewFile();
    void writeBuffer();

    QString m_baseName;
    qint64 m_maxSize;
    int m_maxBackups;
    QFile m_file;
    QByteArray m_buffer;
    QTimer* m_flushTimer;
    int m_lineCount = 0;
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
