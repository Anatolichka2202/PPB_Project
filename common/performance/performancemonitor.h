#ifndef PERFORMANCEMONITOR_H
#define PERFORMANCEMONITOR_H

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QDateTime>
#include <QMetaType>

// Структура для хранения информации о размере очереди
struct QueueSnapshot {
    uint16_t address;
    int size;
    QDateTime timestamp;
};

// Структура для хранения времени выполнения команды
struct CommandTiming {
    QString commandName;
    uint16_t address;
    qint64 elapsedMs;
    Qt::HANDLE threadId;
    QDateTime timestamp;
};

// Регистрация для использования в сигналах/слотах
Q_DECLARE_METATYPE(QueueSnapshot)
Q_DECLARE_METATYPE(CommandTiming)

class PerformanceMonitor : public QObject
{
    Q_OBJECT
public:
    // Синглтон
    static PerformanceMonitor* instance();

    // Логирование событий
    void logQueueSize(uint16_t address, int size);
    void logCommandExecution(const QString& commandName, uint16_t address,
                             qint64 elapsedMs, Qt::HANDLE threadId);

    // Получение последних данных (для отображения в окне)
    QVector<QueueSnapshot> getRecentQueueSnapshots(int maxCount = 100) const;
    QVector<CommandTiming> getRecentCommandTimings(int maxCount = 100) const;

signals:
    void queueUpdated(uint16_t address, int size);
    void commandTimingAdded(const CommandTiming& timing);

private:
    explicit PerformanceMonitor(QObject *parent = nullptr);
    ~PerformanceMonitor();

    static PerformanceMonitor* m_instance;

    mutable QMutex m_mutex;
    QVector<QueueSnapshot> m_queueHistory;
    QVector<CommandTiming> m_commandHistory;
};

#endif // PERFORMANCEMONITOR_H
