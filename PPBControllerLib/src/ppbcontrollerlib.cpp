#include "controller_dependencies.h"
#include "ppbcontrollerlib.h"
#include "icommunication.h"
#include <QDebug>
#include <QThread>
#include <QtEndian>
#include "ds18b20.h"
static struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
        qRegisterMetaType<PPBState>();
        qRegisterMetaType<TechCommand>();
        qRegisterMetaType<DataPacket>();
        qRegisterMetaType<QVector<DataPacket>>();
        qRegisterMetaType<QVector<QByteArray>>();
    }
} _registrar;

// Подключение сигналов коммуникации
void PPBController::connectCommunicationSignals()
{
    if (!m_communication) {
        LOG_TECH_STATE("попытка подключить сигналы к нулевой коммуникации");
        return;
    }

    LOG_TECH_DEBUG("PPBController: подключение сигналов к ICommunication");

    // Подключаем сигналы, используя лямбды при несовпадении сигнатур
    connect(m_communication, &ICommunication::stateChanged,
            this, [this](uint16_t /*address*/, PPBState state) {
                onConnectionStateChanged(state);
            }, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::commandCompleted,
            this, &PPBController::onCommandCompleted, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::commandProgress,
            this, &PPBController::onCommandProgress, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::statusReceived,
            this, &PPBController::onStatusReceived, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::errorOccurred,
            this, &PPBController::onErrorOccurred, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::connected,
            this, [this]() {
                LOG_TECH_DEBUG("PPBController: получен сигнал connected от коммуникации");
                emit connectionStateChanged(PPBState::Ready);
            }, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::disconnected,
            this, [this]() {
                LOG_TECH_DEBUG("PPBController: получен сигнал disconnected от коммуникации");
                emit connectionStateChanged(PPBState::Idle);
            }, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::busyChanged,
            this, &PPBController::onBusyChanged, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::sentPacketsSaved,
            this, &PPBController::onSentPacketsSaved, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::receivedPacketsSaved,
            this, &PPBController::onReceivedPacketsSaved, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::clearPacketDataRequested,
            this, &PPBController::onClearPacketDataRequested, Qt::QueuedConnection);
    connect(m_communication, &ICommunication::commandDataParsed,
            this, &PPBController::onCommandDataParsed, Qt::QueuedConnection);

    connect(m_communication, &ICommunication::commandDataParsed,
            this, &PPBController::onCommandDataParsed);
    connect(m_communication, &ICommunication::groupCommandCompleted,
            this, &PPBController::onGroupCommandCompleted);
}

PPBController::PPBController(ICommunication* communication, PacketAnalyzerInterface* analyzer, QObject *parent)
    : IController(parent)
    , m_communication(communication)
    , m_packetAnalyzer(analyzer)
    , m_autoPollTimer(nullptr)
    , m_autoPollEnabled(false)
    , m_currentAddress(0)
    , busy(false)
{
    LOG_TECH_DEBUG("PPBController: конструктор");

    // Инициализируем таймер автоопроса
    m_autoPollTimer = new QTimer(this);
    m_autoPollTimer->setInterval(5000);
    connect(m_autoPollTimer, &QTimer::timeout, this, &PPBController::onAutoPollTimeout);

    // Подключаем сигналы анализатора
    if (m_packetAnalyzer) {
        connect(m_packetAnalyzer, &PacketAnalyzerInterface::analysisStarted,
                this, &PPBController::onAnalyzerAnalysisStarted);
        connect(m_packetAnalyzer, &PacketAnalyzerInterface::analysisProgress,
                this, &PPBController::onAnalyzerAnalysisProgress);
        connect(m_packetAnalyzer, &PacketAnalyzerInterface::analysisComplete,
                this, &PPBController::onAnalyzerAnalysisComplete);
        connect(m_packetAnalyzer, &PacketAnalyzerInterface::detailedResultsReady,
                this, &PPBController::onAnalyzerDetailedResultsReady);
    }

    // Подключаем сигналы от переданной коммуникации
    if (m_communication) {
        connectCommunicationSignals();

        // Инициируем начальное состояние
        PPBState initialState = m_communication->state();
        LOG_TECH_DEBUG(QString("начальное состояние коммуникации = %1")
                           .arg(static_cast<int>(initialState)));

        emit connectionStateChanged(initialState);
        LOG_UI_RESULT("Контроллер: инициализация завершена");
    } else {
        LOG_TECH_STATE("коммуникация не передана, состояние = Idle");
        emit connectionStateChanged(PPBState::Idle);
    }
}

PPBController::~PPBController()
{
    if (m_autoPollTimer) {
        m_autoPollTimer->stop();
        delete m_autoPollTimer;
    }
}

void PPBController::onBusyChanged(bool busy)
{
    this->busy = busy;
    emit busyChanged(busy);
}

void PPBController::connectToPPB(uint16_t address, const QString& ip, quint16 port)
{
    LOG_UI_OPERATION(QString("PPBController::connectToPPB: address=0x%1, ip=%2, port=%3")
                         .arg(address, 4, 16, QChar('0')).arg(ip).arg(port));

    setCurrentAddress(address);
    if (m_communication) {
        m_communication->connectToPPB(address, ip, port);
    }
    LOG_UI_OPERATION(QString("Подключение к ППБ %1...").arg(address));
}

void PPBController::disconnect()
{
    if (m_communication) {
        m_communication->disconnect();
        LOG_UI_OPERATION("Отключение от ППБ...");
    }
}

void PPBController::requestStatus(uint16_t address)
{
    if (isBusy()) {
        LOG_TECH_STATE("requestStatus: контроллер занят, запрос отклонён");
        return;
    }
    setCurrentAddress(address); // для совместимости, хотя адрес может быть маской
    if (m_communication) {
        // Проверяем, является ли address степенью двойки (один ППБ)
        if ((address & (address - 1)) == 0) {
            // Одиночный адрес
            m_communication->executeCommand(TechCommand::TS, address);
        } else {
            // Множественный адрес – групповая команда
            quint64 groupId = m_communication->executeGroupCommand(TechCommand::TS, 0x0003); // ППБ1 и ППБ2
        }
    }
    LOG_UI_OPERATION(QString("Запрос статуса ППБ %1").arg(address));
}
void PPBController::resetPPB(uint16_t address, const TCDataPayload& payload)
{
    // Старая реализация: просто отправляем то, что пришло (не используем состояние)
    QByteArray data(reinterpret_cast<const char*>(&payload), sizeof(payload));
    if (data.size() < 12) data.resize(12, 0);
    m_communication->executeCommand(TechCommand::TC, address, data);
}

void PPBController::setGeneratorParameters(uint16_t address, uint32_t duration, uint8_t duty, uint32_t delay)
{
    LOG_UI_RESULT(QString("Параметры генератора для ППБ %1: Длительность=%2, Скважность=%3, Задержка=%4")
                      .arg(address).arg(duration).arg(duty).arg(delay));
    // Здесь должна быть отправка команды, но в оригинале нет реализации
}

void PPBController::setFUReceive(uint16_t address, uint16_t duration, uint16_t dutyCycle)
{
    if (m_communication && !m_communication->isBusy()) {
        uint8_t period = (duration >> 8) & 0xFF;
        uint8_t fuData[3] = {
            static_cast<uint8_t>(duration & 0xFF),
            static_cast<uint8_t>((dutyCycle >> 8) & 0xFF),
            static_cast<uint8_t>(dutyCycle & 0xFF)
        };
        m_communication->sendFUReceive(address, period, fuData);
        LOG_UI_RESULT(QString("Режим ФУ прием для ППБ %1: длит=%2, скв=%3")
                          .arg(address).arg(duration).arg(dutyCycle));
    }
}
void PPBController::setFUTransmit(uint16_t address)
{
    if (m_communication && !m_communication->isBusy()) {
        m_communication->sendFUTransmit(address);
        LOG_UI_RESULT(QString("Режим ФУ передача для ППБ %1").arg(address));
    }
}

void PPBController::startPRBS_M2S(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::PRBS_M2S, address);
    }
    LOG_UI_RESULT(QString("Запуск PRBS_M2S для ППБ %1").arg(address));
}

void PPBController::startPRBS_S2M(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::PRBS_S2M, address);
    }
    LOG_UI_RESULT(QString("Запуск PRBS_S2M для ППБ %1").arg(address));
}

void PPBController::runFullTest(uint16_t address)
{
    // TODO: реализовать полный тест
    Q_UNUSED(address);
}

void PPBController::startAutoPoll(int intervalMs)
{
    m_autoPollEnabled = true;
    m_autoPollTimer->start(intervalMs);
    emit autoPollToggled(true);
    LOG_UI_RESULT(QString("Автоопрос включен (интервал %1 мс)").arg(intervalMs));
}

void PPBController::stopAutoPoll()
{
    m_autoPollEnabled = false;
    m_autoPollTimer->stop();
    emit autoPollToggled(false);
    LOG_UI_RESULT("Автоопрос выключен");
}

void PPBController::onCommandDataParsed(uint16_t address, const QVariant& data, TechCommand command)
{
    if (command == TechCommand::IS_YOU) {
        emit commandDataParsed(address, data, command);
        return;
    }

    int index = addressToIndex(address);
    if (index < 0) return;

    if (command == TechCommand::Factory_Number) {
        QVariantMap map = data.toMap();
        if (map.contains("value")) {
            m_ppbStates[index].factoryNumber = map["value"].toUInt();
            emit fullStateUpdated(index);
            emit commandDataParsed(address, data, command);
            LOG_UI_RESULT(QString("Заводской номер ППБ%1: %2").arg(index+1).arg(m_ppbStates[index].factoryNumber));
        }
    }
    // Можно добавить обработку других команд
}

PPBState PPBController::connectionState() const
{
    return m_communication ? m_communication->state() : PPBState::Idle;
}

bool PPBController::isBusy() const
{
    return m_communication ? m_communication->isBusy() : false;
}

bool PPBController::isAutoPollEnabled() const
{
    return m_autoPollEnabled;
}


// ==================== СЛОТЫ ====================

void PPBController::onStatusReceived(uint16_t address, uint32_t mask, const QVector<QByteArray>& data)
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "onStatusReceived",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint16_t, address),
                                  Q_ARG(uint32_t, mask),
                                  Q_ARG(QVector<QByteArray>, data));
        return;
    }

    if (m_currentAddress == 0) {
        setCurrentAddress(address);
    }

    processStatusData(address, mask, data);
    emit statusReceived(address, data);
}

void PPBController::onConnectionStateChanged(PPBState state)
{
    emit connectionStateChanged(state);
}

void PPBController::onCommandProgress(int current, int total, TechCommand command)
{
    QString operation = commandToName(command);
    emit operationProgress(current, total, operation);
    LOG_TECH_STATE(QString("%1: %2/%3").arg(operation).arg(current).arg(total));
}

void PPBController::onCommandCompleted(bool success, const QString& message, TechCommand command)
{
    QString logMsg = QString("Команда %1: %2")
                         .arg(commandToName(command))
                         .arg(message);

    if (success) {
        LOG_UI_RESULT(logMsg);
        if (command == TechCommand::TS) {
            emit connectionStateChanged(PPBState::Ready);
        }
    } else {
        LOG_UI_ALERT(logMsg);
        emit errorOccurred(message);
        LOG_UI_ALERT("Ошибка: " + message);
    }

    emit operationCompleted(success, message);
    emit commandCompleted(success, message, command);
}

void PPBController::onErrorOccurred(const QString& error)
{
    emit errorOccurred(error);
    LOG_UI_ALERT("[ОШИБКА] " + error);
}

void PPBController::onAutoPollTimeout()
{
    if (m_autoPollEnabled && m_communication &&
        m_communication->state() == PPBState::Ready && m_currentAddress != 0) {
        requestStatus(m_currentAddress);
    }
}

void PPBController::requestFabricNumber(uint16_t address) {
    if (m_communication) {
        m_communication->executeCommand(TechCommand::Factory_Number, address);
        LOG_UI_OPERATION(QString("Запрос заводского номера ППБ %1").arg(address));
    }
}

// ==================== АНАЛИЗ ПАКЕТОВ ====================

void PPBController::saveSentPackets(const QVector<DataPacket>& packets) {
    m_lastSentPackets = packets;
    if (m_packetAnalyzer) {
        m_packetAnalyzer->addSentPackets(packets);
    }
    LOG_TECH_DEBUG(QString("Сохранено %1 отправленных пакетов").arg(packets.size()));
}

void PPBController::saveReceivedPackets(const QVector<DataPacket>& packets) {
    m_lastReceivedPackets = packets;
    if (m_packetAnalyzer) {
        m_packetAnalyzer->addReceivedPackets(packets);
    }
    LOG_TECH_DEBUG(QString("Сохранено %1 полученных пакетов").arg(packets.size()));
}

void PPBController::onSentPacketsSaved(const QVector<DataPacket>& packets) {
    LOG_TECH_DEBUG(QString("Получены отправленные пакеты: %1 шт").arg(packets.size()));
    saveSentPackets(packets);
    LOG_UI_STATUS(QString("Сохранено %1 отправленных пакетов").arg(packets.size()));
}

void PPBController::onReceivedPacketsSaved(const QVector<DataPacket>& packets) {
    LOG_TECH_DEBUG(QString("Получены принятые пакеты: %1 шт").arg(packets.size()));
    saveReceivedPackets(packets);
    LOG_UI_STATUS(QString("Сохранено %1 полученных пакетов").arg(packets.size()));
}

void PPBController::onClearPacketDataRequested() {
    LOG_TECH_DEBUG("Запрос на очистку данных пакетов");
    if (m_packetAnalyzer) {
        m_packetAnalyzer->clear();
    }
    m_lastSentPackets.clear();
    m_lastReceivedPackets.clear();
}

void PPBController::analize() {
    LOG_TECH_DEBUG("=== АНАЛИЗ ПАКЕТОВ ===");

    if (!m_packetAnalyzer) {
        LOG_UI_ALERT("Анализатор не инициализирован");
        return;
    }

    int sentCount = m_packetAnalyzer->sentCount();
    int receivedCount = m_packetAnalyzer->receivedCount();

    if (sentCount == 0 && receivedCount == 0) {
        LOG_UI_STATUS("Нет данных для анализа");
        LOG_TECH_DEBUG("Отсутствуют отправленные и полученные пакеты");
        return;
    }

    if (sentCount == 0) {
        LOG_UI_STATUS("Нет отправленных пакетов");
        if (!m_lastReceivedPackets.isEmpty()) {
            showPacketsTable("Полученные пакеты", m_lastReceivedPackets);
        }
        return;
    }

    if (receivedCount == 0) {
        LOG_UI_STATUS("Нет полученных пакетов");
        if (!m_lastSentPackets.isEmpty()) {
            showPacketsTable("Отправленные пакеты", m_lastSentPackets);
        }
        return;
    }

    m_packetAnalyzer->analyze();
}

void PPBController::onAnalyzerAnalysisStarted() {
    LOG_TECH_DEBUG("Анализатор начал работу");
    emit analysisStarted();
}

void PPBController::onAnalyzerAnalysisProgress(int percent) {
    emit analysisProgress(percent);
}

void PPBController::onAnalyzerAnalysisComplete(const QString& summary) {
    LOG_TECH_DEBUG("Анализатор завершил работу");
    // summary может быть пустым, но мы показываем результаты через detailedResults
}

void PPBController::onAnalyzerDetailedResultsReady(const QVariantMap& results) {
    LOG_TECH_DEBUG("Получены детальные результаты анализа");
    QString summary = results.value("summary", "").toString();
    showAnalysisResults(summary, results);
    emit analysisComplete(summary, results);
}

void PPBController::showAnalysisResults(const QString& summary, const QVariantMap& details) {
    // Реализация показа результатов анализа
    CardData summaryCard;
    summaryCard.id = "analysis-summary";
    summaryCard.title = "📊 Результаты анализа";
    summaryCard.backgroundColor = QColor(240, 248, 255);

    summaryCard.addField("Отправлено", details["totalSent"].toString());
    summaryCard.addField("Получено", details["totalReceived"].toString());
    summaryCard.addField("Потери", details["lostPackets"].toString());

    if (details.contains("packetLossRate")) {
        double lossRate = details["packetLossRate"].toDouble();
        summaryCard.addField("Потери (%)", QString::number(lossRate * 100, 'f', 2) + "%");
    }

    summaryCard.addField("Ошибок CRC", details["crcErrors"].toString());
    summaryCard.addField("Битовых ошибок", details["bitErrors"].toString());

    if (details.contains("ber")) {
        double ber = details["ber"].toDouble();
        summaryCard.addField("BER", QString::number(ber, 'e', 6));
    }

    LOG_UI_CARD(summaryCard);

    // Детальная таблица сравнения
    if (details.contains("errorDetails")) {
        QVariantList errorDetails = details["errorDetails"].toList();
        if (!errorDetails.isEmpty()) {
            TableData detailsTable;
            detailsTable.id = "analysis-details";
            detailsTable.title = "Детали сравнения пакетов";
            detailsTable.headers = QStringList() << "Индекс" << "Отправлено" << "Получено" << "Статус" << "Битовые ошибки";

            for (const auto& item : errorDetails) {
                QVariantMap detail = item.toMap();
                QString status;
                if (detail["isLost"].toBool()) {
                    status = "🔴 ПОТЕРЯН";
                } else if (detail["hasCrcError"].toBool()) {
                    status = "⚠️ ОШИБКА CRC";
                } else if (detail["isOutOfOrder"].toBool()) {
                    status = "↕️ НЕ В ПОРЯДКЕ";
                } else if (detail["bitErrors"].toInt() > 0) {
                    status = QString("⚡ %1 бит").arg(detail["bitErrors"].toInt());
                } else {
                    status = "✅ OK";
                }

                detailsTable.addRow({
                    detail["index"].toString(),
                    detail["sentData"].toString(),
                    detail["receivedData"].toString(),
                    status,
                    detail["bitErrors"].toString()
                });
            }

            LOG_UI_TABLE(detailsTable);
        }
    }

    if (!summary.isEmpty()) {
        LOG_TECH_DEBUG(summary);
    }
}

void PPBController::showPacketsTable(const QString& title, const QVector<DataPacket>& packets) {
    TableData table;
    table.id = "packets-table";
    table.title = title;
    table.headers = {"Индекс", "Данные [0]", "Данные [1]", "HEX представление"};
    table.columnFormats[0] = "hex";
    table.columnFormats[1] = "hex";
    table.columnFormats[2] = "hex";

    for (int i = 0; i < packets.size(); ++i) {
        const DataPacket& packet = packets[i];
        table.addRow({
            QString::number(i),
            QString::number(packet.data[0], 16).rightJustified(2, '0').toUpper(),
            QString::number(packet.data[1], 16).rightJustified(2, '0').toUpper(),
            QString("[%1 %2]")
                .arg(packet.data[0], 2, 16, QChar('0'))
                .arg(packet.data[1], 2, 16, QChar('0'))
        });
    }

    LOG_UI_TABLE(table);
    LOG_UI_STATUS(QString("Показано %1 пакетов").arg(packets.size()));
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================

void PPBController::processStatusData(uint16_t address, uint32_t mask, const QVector<QByteArray>& blocks)
{
    int index = addressToIndex(address);
    if (index < 0) return;

    PPBFullState& state = m_ppbStates[index];
    state.addressMask = address;
    state.statusMask = static_cast<uint16_t>(mask); // если нужно хранить как 16 бит, но лучше расширить поле в PPBFullState до uint32_t
    // Временно оставим так, но позже нужно изменить тип statusMask в PPBFullState на uint32_t.

    int blockIdx = 0;

    // Вспомогательные переменные для сборки 4-байтовых слов
    uint32_t power1_high = 0; bool have_power1_high = false;
    uint32_t power2_high = 0; bool have_power2_high = false;
    uint32_t vswr1_high = 0; bool have_vswr1_high = false;
    uint32_t vswr2_high = 0; bool have_vswr2_high = false;

    for (int bit = 0; bit <= 23; ++bit) { // до 23, но реально используем 0..16
        if (!(mask & (1 << bit))) continue;
        if (blockIdx >= blocks.size()) break;

        const QByteArray& block = blocks[blockIdx];
        blockIdx++;

        if (block.size() < 2) continue;

        uint16_t word = qFromBigEndian<uint16_t>(block.constData());

        switch (bit) {
        case 0: // Состояние (флаги каналов)
            state.ch1.isOk = (static_cast<uint8_t>(block[0]) != 0);
            state.ch2.isOk = (static_cast<uint8_t>(block[1]) != 0);
            // Здесь можно также извлечь другие флаги, если они есть в этом блоке
            break;

        case 1: // Мощность канала 1, старшая часть
            power1_high = word;
            have_power1_high = true;
            break;
        case 2: // Мощность канала 1, младшая часть
            if (have_power1_high) {
                uint32_t full = (power1_high << 16) | word;
                state.ch1.power = DataConverter::codeToPower(full);
                have_power1_high = false;
            } else {
                LOG_UI_ALERT("Ошибка парсинга мощности канала 1: пропущена старшая часть");
            }
            break;

        case 3: // Мощность канала 2, старшая часть
            power2_high = word;
            have_power2_high = true;
            break;
        case 4: // Мощность канала 2, младшая часть
            if (have_power2_high) {
                uint32_t full = (power2_high << 16) | word;
                state.ch2.power = DataConverter::codeToPower(full);
                have_power2_high = false;
            } else {
                LOG_UI_ALERT("Ошибка парсинга мощности канала 2: пропущена старшая часть");
            }
            break;

        case 5: // Температура t1
            state.tempT1 = ds18b20::convert(static_cast<int16_t>(word));
            break;
        case 6: // t2
            state.tempT2 = ds18b20::convert(static_cast<int16_t>(word));
            break;
        case 7: // t3
            state.tempT3 = ds18b20::convert(static_cast<int16_t>(word));
            break;
        case 8: // t4
            state.tempT4 = ds18b20::convert(static_cast<int16_t>(word));
            break;
        case 9: // tv1
            state.tempV1 = ds18b20::convert(static_cast<int16_t>(word));
            break;
        case 10: // tv2
            state.tempV2 = ds18b20::convert(static_cast<int16_t>(word));
            break;
        case 11: // tin
            state.tempIn = ds18b20::convert(static_cast<int16_t>(word));
            break;
        case 12: // tout
            state.tempOut = ds18b20::convert(static_cast<int16_t>(word));
            break;

        case 13: // КСВН канала 1, старшая часть
            vswr1_high = word;
            have_vswr1_high = true;
            break;
        case 14: // КСВН канала 1, младшая часть
            if (have_vswr1_high) {
                uint32_t full = (vswr1_high << 16) | word;
                state.ch1.vswr = DataConverter::codeToVSWR(full);
                have_vswr1_high = false;
            } else {
                LOG_UI_ALERT("Ошибка парсинга КСВН канала 1: пропущена старшая часть");
            }
            break;

        case 15: // КСВН канала 2, старшая часть
            vswr2_high = word;
            have_vswr2_high = true;
            break;
        case 16: // КСВН канала 2, младшая часть
            if (have_vswr2_high) {
                uint32_t full = (vswr2_high << 16) | word;
                state.ch2.vswr = DataConverter::codeToVSWR(full);
                have_vswr2_high = false;
            } else {
                LOG_UI_ALERT("Ошибка парсинга КСВН канала 2: пропущена старшая часть");
            }
            break;

        default:
            // Неизвестный бит – игнорируем
            break;
        }
    }

    emit fullStateUpdated(index);
}

QString PPBController::commandToName(TechCommand command) const
{
    static QMap<TechCommand, QString> names = {
        {TechCommand::TS, "Опрос состояния"},
        {TechCommand::TC, "Сброс"},
        {TechCommand::VERS, "Запрос версии"},
        {TechCommand::VOLUME, "Принять том ПО"},
        {TechCommand::CHECKSUM, "Контрольная сумма"},
        {TechCommand::PROGRAMM, "Обновление ПО"},
        {TechCommand::CLEAN, "Очистка"},
        {TechCommand::DROP, "Отброшенные пакеты"},
        {TechCommand::PRBS_M2S, "PRBS передача"},
        {TechCommand::PRBS_S2M, "PRBS приём"},
        {TechCommand::BER_T, "BER ТУ"},
        {TechCommand::BER_F, "BER ФУ"},
        {TechCommand::IS_YOU, "Прозвон сети"},
    };
    return names.value(command, "Неизвестная команда");
}

void PPBController::exucuteCommand(TechCommand tech, uint16_t address)
{
    if(m_communication) {m_communication->executeCommand(tech,address);}

}

void PPBController::requestVersion(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::VERS, address);
        LOG_UI_OPERATION(QString("Запрос версии ППБ %1").arg(address));
    }
}

void PPBController::requestVolume(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::VOLUME, address);
        LOG_UI_OPERATION(QString("Запрос тома ПО ППБ %1").arg(address));
    }
}

void PPBController::requestChecksum(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::CHECKSUM, address);
        LOG_UI_OPERATION(QString("Запрос контрольной суммы ППБ %1").arg(address));
    }
}

void PPBController::sendProgram(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::PROGRAMM, address);
        LOG_UI_OPERATION(QString("Обновление ПО ППБ %1").arg(address));
    }
}

void PPBController::sendClean(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::CLEAN, address);
        LOG_UI_OPERATION(QString("Очистка временного файла ПО ППБ %1").arg(address));
    }
}

void PPBController::requestDroppedPackets(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::DROP, address);
        LOG_UI_OPERATION(QString("Запрос отброшенных пакетов ППБ %1").arg(address));
    }
}

void PPBController::requestBER_T(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::BER_T, address);
        LOG_UI_OPERATION(QString("Запрос BER ТУ ППБ %1").arg(address));
    }
}

void PPBController::requestBER_F(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::BER_F, address);
        LOG_UI_OPERATION(QString("Запрос BER ФУ ППБ %1").arg(address));
    }
}

void PPBController::setCommunication(ICommunication* communication)
{
    LOG_TECH_DEBUG("PPBController::setCommunication");

    if (m_communication == communication) {
        LOG_TECH_DEBUG("PPBController::setCommunication: тот же объект, игнорируем");
        return;
    }

    // Очищаем старое соединение
    if (m_communication) {
        if (m_autoPollTimer && m_autoPollTimer->isActive()) {
            m_autoPollTimer->stop();
        }

        QObject::disconnect(m_communication, nullptr, this, nullptr);

        if (m_communication->state() == PPBState::Ready) {
            m_communication->disconnect();
        }

        m_communication = nullptr;

        m_currentAddress = 0;


        emit connectionStateChanged(PPBState::Idle);
        LOG_TECH_STATE("Коммуникационный объект заменен");
    }

    m_communication = communication;

    if (m_communication) {
        LOG_TECH_DEBUG("PPBController::setCommunication: настраиваем новый объект");
        connectCommunicationSignals();

        PPBState newState = m_communication->state();
        LOG_TECH_DEBUG(QString("PPBController::setCommunication: состояние нового объекта = %1")
                           .arg(static_cast<int>(newState)));

        emit connectionStateChanged(newState);

        if (m_autoPollEnabled && m_autoPollTimer) {
            m_autoPollTimer->start();
        }

        LOG_TECH_STATE("PPBController: коммуникационный объект успешно заменен");
    } else {
        LOG_TECH_STATE("PPBController::setCommunication: передан nullptr");
    }
}

void PPBController::setChannelPower(uint8_t ppbIndex, int channel, float watts)
{
    auto& state = m_ppbStates[ppbIndex];
    if (channel == 1)
        state.ch1.power = watts;
    else if (channel == 2)
        state.ch2.power = watts;
    else
        return;
    emit fullStateUpdated(ppbIndex);
}

void PPBController::setFuBlocked(uint8_t ppbIndex, bool blocked)
{
    m_ppbStates[ppbIndex].fuBlocked = blocked;
    emit fullStateUpdated(ppbIndex);
}

void PPBController::setRebootRequested(uint8_t ppbIndex, bool reboot)
{
    m_ppbStates[ppbIndex].rebootRequested = reboot;
    emit fullStateUpdated(ppbIndex);
}

void PPBController::setResetErrors(uint8_t ppbIndex, bool reset)
{
    m_ppbStates[ppbIndex].resetErrors = reset;
    emit fullStateUpdated(ppbIndex);
}

void PPBController::sendTC(uint16_t address)
{
    int index = addressToIndex(address);
    if (index < 0) return;
    const auto& state = m_ppbStates[index];

    TCDataPayload payload = {};  // обнуляем
    payload.power1 = DataConverter::powerToCode(state.ch1.power);
    payload.power2 = DataConverter::powerToCode(state.ch2.power);
    payload.stateMask = 0;
    if (state.fuBlocked) payload.stateMask |= 0x01;
    if (state.rebootRequested) payload.stateMask |= 0x02;
    if (state.resetErrors) payload.stateMask |= 0x04;
    if (state.powerEnabled) payload.stateMask |= 0x80;
    // Упаковка в QByteArray с преобразованием в big-endian
    QByteArray data;
    data.resize(sizeof(payload));
    uint32_t* p1 = reinterpret_cast<uint32_t*>(data.data());
    uint32_t* p2 = reinterpret_cast<uint32_t*>(data.data() + 4);
    uint8_t* mask = reinterpret_cast<uint8_t*>(data.data() + 8);
    uint8_t* res = reinterpret_cast<uint8_t*>(data.data() + 9);

    *p1 = qToBigEndian(payload.power1);
    *p2 = qToBigEndian(payload.power2);
    *mask = payload.stateMask;
    memset(res, 0, 3);  // reserved

    m_communication->executeCommand(TechCommand::TC, address, data);
}

void PPBController::setCurrentAddress(uint16_t address)
{
    if (m_currentAddress != address) {
        LOG_UI_CONNECTION(QString("PPBController: изменение текущего адреса: 0x%1 -> 0x%2")
                              .arg(m_currentAddress, 4, 16, QChar('0'))
                              .arg(address, 4, 16, QChar('0')));
        m_currentAddress = address;
    }
}

PPBFullState PPBController::getFullState(uint8_t ppbIndex) const
{
    return m_ppbStates.value(ppbIndex);
}


//++++++++++++++++++++++++++++++++++AKIP++++++++++++++++++++++++++++++++++++++++
void PPBController::setAkipInterface(IAkipController* akip)
{
    m_akip = akip;
    if (m_akip) {
        connect(m_akip, &IAkipController::availabilityChanged, this, &PPBController::akipAvailabilityChanged);
    }
}

void PPBController::setAkipFrequency(int channel, double freqHz)
{
    if (m_akip) m_akip->setFrequency(channel, freqHz);
}

void PPBController::setAkipAmplitude(int channel, double value, const QString &unit)
{
    if (m_akip) m_akip->setAmplitude(channel, value, unit);
}

void PPBController::setAkipOutput(int channel, bool enable)
{
    if (m_akip) m_akip->setOutput(channel, enable);
}

void PPBController::setAkipWaveform(int channel, const QString &wave)
{
    if (m_akip) m_akip->setWaveform(channel, wave);
}

void PPBController::setAkipDutyCycle(int channel, double percent)
{
    if (m_akip) m_akip->setDutyCycle(channel, percent);
}
void PPBController::setPowerEnabled(uint8_t ppbIndex, bool enabled)
{
    if (ppbIndex < 16) {
        m_ppbStates[ppbIndex].powerEnabled = enabled;
        emit fullStateUpdated(ppbIndex);
    }
}

void PPBController::setBridgeAddress(const QString &ip, quint16 port)
{
    if (m_communication)
        m_communication->setBridgeAddress(ip, port);
}

void PPBController::onGroupCommandCompleted(quint64 groupId, bool allSuccess, const QString& summary)
{
    emit groupCommandCompleted(groupId, allSuccess, summary);
}
