#include "controller_dependencies.h"
#include "ppbcontrollerlib.h"
#include "icommunication.h"
#include <QDebug>
#include <QThread>
#include <QtEndian>

static struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
        qRegisterMetaType<PPBState>();
        qRegisterMetaType<TechCommand>();
        qRegisterMetaType<DataPacket>();
        qRegisterMetaType<UIChannelState>();
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

    // Инициализируем карты состояний
    m_channel1States.clear();
    m_channel2States.clear();

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
    setCurrentAddress(address);
    if (m_communication) {
        m_communication->executeCommand(TechCommand::TS, address);
    }
    LOG_UI_OPERATION(QString("Запрос статуса ППБ %1").arg(address));
}

void PPBController::resetPPB(uint16_t address)
{
    if (m_communication) {
        m_communication->executeCommand(TechCommand::TC, address);
    }
    LOG_UI_OPERATION(QString("Сброс ППБ %1").arg(address));
}

void PPBController::setGeneratorParameters(uint16_t address, uint32_t duration, uint8_t duty, uint32_t delay)
{
    LOG_UI_RESULT(QString("Параметры генератора для ППБ %1: Длительность=%2, Скважность=%3, Задержка=%4")
                      .arg(address).arg(duration).arg(duty).arg(delay));
    // Здесь должна быть отправка команды, но в оригинале нет реализации
}

void PPBController::setFUReceive(uint16_t address, uint8_t period)
{
    if (m_communication && !m_communication->isBusy()) {
        m_communication->sendFUReceive(address, period);
        LOG_UI_RESULT(QString("Режим ФУ прием для ППБ %1").arg(address));
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

UIChannelState PPBController::getChannelState(uint8_t ppbIndex, int channel) const
{
    if (channel == 1) {
        return m_channel1States.value(ppbIndex);
    } else {
        return m_channel2States.value(ppbIndex);
    }
}

// ==================== СЛОТЫ ====================

void PPBController::onStatusReceived(uint16_t address, uint16_t mask, const QVector<QByteArray>& data)
{
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, "onStatusReceived",
                                  Qt::QueuedConnection,
                                  Q_ARG(uint16_t, address),
                                  Q_ARG(uint16_t, mask),
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
            detailsTable.headers = {"Индекс", "Отправлено", "Получено", "Статус", "Битовые ошибки"};

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

void PPBController::processStatusData(uint16_t address, uint16_t mask, const QVector<QByteArray>& data)
{
    if (data.size() < 1) {
        LOG_TECH_STATE("processStatusData: нет данных");
        return;
    }

    int index = -1;
    switch (address) {
    case 0x0001: index = 0; break;
    case 0x0002: index = 1; break;
    case 0x0004: index = 2; break;
    case 0x0008: index = 3; break;
    case 0x0010: index = 4; break;
    case 0x0020: index = 5; break;
    case 0x0040: index = 6; break;
    case 0x0080: index = 7; break;
    default:
        LOG_TECH_STATE("processStatusData: неизвестный адрес " + QString::number(address, 16));
        return;
    }

    // Инициализация с дефолтными значениями (все поля как "нет данных")
    UIChannelState ch1, ch2;
    ch1.isOk = false;
    ch2.isOk = false;
    ch1.power = 0.0f;
    ch2.power = 0.0f;
    ch1.temperature = 0.0f;
    ch2.temperature = 0.0f;
    ch1.vswr = 1.0f;
    ch2.vswr = 1.0f;
    // остальные поля тоже обнуляем

    // Проходим по битам маски (бит 0 соответствует первому пакету после маски)
    int packetIdx = 0;
    for (int bit = 0; bit < 9; ++bit) {
        if (!(mask & (1 << bit))) continue;
        if (packetIdx >= data.size()) break;

        const QByteArray& pkt = data[packetIdx];
        packetIdx++;

        if (pkt.size() < 2) continue;

        switch (bit) {
        case 0: // флаги состояния каналов
        {
            uint8_t flags1 = static_cast<uint8_t>(pkt.at(0));
            uint8_t flags2 = static_cast<uint8_t>(pkt.at(1));
            ch1.isOk = (flags1 != 0);
            ch2.isOk = (flags2 != 0);
            break;
        }
        case 1: // мощность канала 1
        {
            uint16_t code =  qFromBigEndian<uint16_t>(pkt.constData());
            ch1.power = DataConverter::codeToPower(code);
            break;
        }
        case 2: // мощность канала 2
        {
            uint16_t code =  qFromBigEndian<uint16_t>(pkt.constData());
            ch2.power = DataConverter::codeToPower(code);
            break;
        }
        case 3: // температуры t1, t2
        {
            ch1.tempT1 = DataConverter::codeToTemperature(static_cast<int8_t>(pkt.at(0)));
            ch1.tempT2 = DataConverter::codeToTemperature(static_cast<int8_t>(pkt.at(1)));
            break;
        }
        case 4: // температуры t3, t4
        {
            ch1.tempT3 = DataConverter::codeToTemperature(static_cast<int8_t>(pkt.at(0)));
            ch1.tempT4 = DataConverter::codeToTemperature(static_cast<int8_t>(pkt.at(1)));
            break;
        }
        case 5: // температуры v1, v2
        {
            ch1.tempV1 = DataConverter::codeToTemperature(static_cast<int8_t>(pkt.at(0)));
            ch1.tempV2 = DataConverter::codeToTemperature(static_cast<int8_t>(pkt.at(1)));
            break;
        }
        case 6: // температуры out, in
        {
            ch1.tempOut = DataConverter::codeToTemperature(static_cast<int8_t>(pkt.at(0)));
            ch1.tempIn  = DataConverter::codeToTemperature(static_cast<int8_t>(pkt.at(1)));
            break;
        }
        case 7: // КСВН канала 1
        {
            uint16_t code =  qFromBigEndian<uint16_t>(pkt.constData());
            ch1.vswr = DataConverter::codeToVSWR(code);
            break;
        }
        case 8: // КСВН канала 2
        {
            uint16_t code =  qFromBigEndian<uint16_t>(pkt.constData());
            ch2.vswr = DataConverter::codeToVSWR(code);
            break;
        }
        default:
            break;
        }
    }

    // Копируем общие температуры из канала 1 в канал 2 (они общие для ППБ)
    ch2.tempT1 = ch1.tempT1;
    ch2.tempT2 = ch1.tempT2;
    ch2.tempT3 = ch1.tempT3;
    ch2.tempT4 = ch1.tempT4;
    ch2.tempV1 = ch1.tempV1;
    ch2.tempV2 = ch1.tempV2;
    ch2.tempOut = ch1.tempOut;
    ch2.tempIn  = ch1.tempIn;

    // Сохраняем состояния
    m_channel1States[index] = ch1;
    m_channel2States[index] = ch2;

    emit channelStateUpdated(index, 1, ch1);
    emit channelStateUpdated(index, 2, ch2);

    LOG_TECH_STATE(QString("Статус ППБ%1 обновлен (маска 0x%2)").arg(index + 1).arg(mask, 4, 16, QChar('0')));
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
        {TechCommand::BER_F, "BER ФУ"}
    };
    return names.value(command, "Неизвестная команда");
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
        m_channel1States.clear();
        m_channel2States.clear();

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

void PPBController::setCurrentAddress(uint16_t address)
{
    if (m_currentAddress != address) {
        LOG_UI_CONNECTION(QString("PPBController: изменение текущего адреса: 0x%1 -> 0x%2")
                              .arg(m_currentAddress, 4, 16, QChar('0'))
                              .arg(address, 4, 16, QChar('0')));
        m_currentAddress = address;
    }
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
