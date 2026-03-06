#include "performancemonitor.h"

PerformanceMonitor* PerformanceMonitor::m_instance = nullptr;


PerformanceMonitor* PerformanceMonitor::instance()
{
    if (!m_instance) {
        m_instance = new PerformanceMonitor();
    }
    return m_instance;
}

PerformanceMonitor::PerformanceMonitor(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QueueSnapshot>();
    qRegisterMetaType<CommandTiming>();
}

PerformanceMonitor::~PerformanceMonitor()
{
    m_instance = nullptr;
}

void PerformanceMonitor::logQueueSize(uint16_t address, int size)
{
    {
        QMutexLocker locker(&m_mutex);
        QueueSnapshot snap;
        snap.address = address;
        snap.size = size;
        snap.timestamp = QDateTime::currentDateTime();
        m_queueHistory.append(snap);
        if (m_queueHistory.size() > 1000)
            m_queueHistory.removeFirst();
    }
    emit queueUpdated(address, size);
}

void PerformanceMonitor::logCommandExecution(const QString& commandName, uint16_t address,
                                             qint64 elapsedMs, Qt::HANDLE threadId)
{
    CommandTiming timing;  // объявляем здесь
    timing.commandName = commandName;
    timing.address = address;
    timing.elapsedMs = elapsedMs;
    timing.threadId = threadId;
    timing.timestamp = QDateTime::currentDateTime();

    {
        QMutexLocker locker(&m_mutex);
        m_commandHistory.append(timing);
        if (m_commandHistory.size() > 1000)
            m_commandHistory.removeFirst();
    }
    emit commandTimingAdded(timing);
}

QVector<QueueSnapshot> PerformanceMonitor::getRecentQueueSnapshots(int maxCount) const
{
    QMutexLocker locker(&m_mutex);
    if (m_queueHistory.size() <= maxCount)
        return m_queueHistory;
    return m_queueHistory.mid(m_queueHistory.size() - maxCount);
}

QVector<CommandTiming> PerformanceMonitor::getRecentCommandTimings(int maxCount) const
{
    QMutexLocker locker(&m_mutex);
    if (m_commandHistory.size() <= maxCount)
        return m_commandHistory;
    return m_commandHistory.mid(m_commandHistory.size() - maxCount);
}
