#include "abstractscpicontroller.h"
#include <QThread>
#include <QElapsedTimer>

AbstractScpiController::AbstractScpiController(QObject *parent)
    : IAkipController(parent)
{
}

AbstractScpiController::~AbstractScpiController()
{
}

bool AbstractScpiController::ensureAvailable() const
{
    if (!isTransportAvailable()) {
        const_cast<AbstractScpiController*>(this)->emit errorOccurred("Device not available");
        return false;
    }
    return true;
}

bool AbstractScpiController::verifySetCommand(const QString &cmd,
                                              std::function<double()> queryFunc,
                                              double expectedValue,
                                              double tolerance,
                                              int maxRetries,
                                              int delayMs)
{
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        // 1. Отправляем команду установки
        if (!sendCommand(cmd)) {
            if (attempt < maxRetries - 1) {
                QThread::msleep(delayMs);
                continue;
            } else {
                emit errorOccurred("Failed to send command after retries");
                return false;
            }
        }

        // 2. Небольшая задержка для применения
        QThread::msleep(200);

        // 3. Запрашиваем фактическое значение
        double actual = queryFunc();
        if (qAbs(actual - expectedValue) <= tolerance)
            return true;

        // 4. Если не совпало — запрашиваем ошибку прибора
        QString sysErr = querySystemError();
        emit errorOccurred(QString("Verification failed: expected %1, got %2. System error: %3")
                               .arg(expectedValue).arg(actual).arg(sysErr));
    }
    return false;
}

bool AbstractScpiController::verifySetCommand(const QString &cmd,
                                              std::function<bool()> queryFunc,
                                              bool expectedValue,
                                              int maxRetries,
                                              int delayMs)
{
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        // 1. Отправляем команду установки (не ожидаем ответа)
        if (!sendCommand(cmd)) {
            if (attempt < maxRetries - 1) {
                QThread::msleep(delayMs);
                continue;
            } else {
                emit errorOccurred("Failed to send command after retries");
                return false;
            }
        }

        // 2. Небольшая задержка для применения
        QThread::msleep(200);

        // 3. Запрашиваем фактическое значение через queryFunc
        bool actual = queryFunc();
        if (actual == expectedValue)
            return true;

        // 4. Если не совпало — запрашиваем системную ошибку
        QString sysErr = querySystemError();
        emit errorOccurred(QString("Verification failed: expected %1, got %2. System error: %3")
                               .arg(expectedValue).arg(actual).arg(sysErr));
    }
    return false;
}

void AbstractScpiController::registerCommand(const QString &key, const CommandDef &def)
{
    m_commandMap.insert(key, def);
}

bool AbstractScpiController::checkRange(const QString &key, double val) const
{
    auto it = m_commandMap.find(key);
    if (it == m_commandMap.end()) {
        emit const_cast<AbstractScpiController*>(this)->errorOccurred(
            QString("Unknown command key: %1").arg(key));
        return false;
    }
    const CommandDef &def = it.value();
    if (def.isBoolean) {
        return true; // для булевых команд диапазон не проверяем
    }
    if (val < def.minVal || val > def.maxVal) {
        emit const_cast<AbstractScpiController*>(this)->errorOccurred(
            QString("Value %1 out of range [%2, %3] for command %4")
                .arg(val).arg(def.minVal).arg(def.maxVal).arg(key));
        return false;
    }
    return true;
}

QString AbstractScpiController::buildSetCommand(const QString &key, double val) const
{
    auto it = m_commandMap.find(key);
    if (it == m_commandMap.end()) {
        emit const_cast<AbstractScpiController*>(this)->errorOccurred(
            QString("Unknown command key: %1").arg(key));
        return QString();
    }
    const CommandDef &def = it.value();
    // Если значение целое (с точностью до 1e-9), форматируем без десятичных знаков
    if (qAbs(val - int(val)) < 1e-9)
        return def.setPattern.arg(int(val));
    else
        return def.setPattern.arg(val, 0, 'f', 4); // для нецелых оставляем 4 знака
}


QString AbstractScpiController::buildSetCommand(const QString &key, bool val) const
{
    auto it = m_commandMap.find(key);
    if (it == m_commandMap.end()) {
        emit const_cast<AbstractScpiController*>(this)->errorOccurred(
            QString("Unknown command key: %1").arg(key));
        return QString();
    }
    const CommandDef &def = it.value();
    return def.setPattern.arg(val ? "ON" : "OFF");
}

QString AbstractScpiController::buildQueryCommand(const QString &key) const
{
    auto it = m_commandMap.find(key);
    if (it == m_commandMap.end()) {
        emit const_cast<AbstractScpiController*>(this)->errorOccurred(
            QString("Unknown command key: %1").arg(key));
        return QString();
    }
    return it.value().queryCmd;
}

bool AbstractScpiController::sendCommandTimed(const QString &cmd, qint64 &elapsedMs)
{
    QElapsedTimer timer;
    timer.start();
    bool ok = sendCommand(cmd);
    elapsedMs = timer.elapsed();
    return ok;
}

bool AbstractScpiController::queryCommandTimed(const QString &cmd, QString &response, qint64 &elapsedMs)
{
    QElapsedTimer timer;
    timer.start();
    response = queryCommand(cmd);
    elapsedMs = timer.elapsed();
    return !response.isEmpty();
}
