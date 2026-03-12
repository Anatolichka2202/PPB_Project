#include "commandandoperation.h"

#include "../comm_dependencies.h"
#include <QDataStream>
#include <QtEndian>


// Реализация метода create
std::unique_ptr<PPBCommand> CommandFactory::create(TechCommand cmd) {
    switch (cmd) {
    case TechCommand::TS: return std::make_unique<StatusCommand>();
    //case TechCommand::TC: return std::make_unique<ResetCommand>();
    case TechCommand::VERS: return std::make_unique<VersCommand>();
    case TechCommand::VOLUME: return std::make_unique<VolumeCommand>();
    case TechCommand::CHECKSUM: return std::make_unique<CheckSumCommand>();
    case TechCommand::PROGRAMM: return std::make_unique<ProgrammCommand>();
    case TechCommand::CLEAN: return std::make_unique<CleanCommand>();
    case TechCommand::DROP: return std::make_unique<DROPCommand>();
    case TechCommand::PRBS_M2S: return std::make_unique<PRBS_M2SCommand>();
    case TechCommand::PRBS_S2M: return std::make_unique<PRBS_S2MCommand>();
    case TechCommand::BER_T: return std::make_unique<BER_TCommand>();
    case TechCommand::BER_F: return std::make_unique<BER_FCommand>();
    case TechCommand::Factory_Number: return std::make_unique<Factory_Number>();
    default: return nullptr;
    }
}

QString CommandFactory::commandName(TechCommand cmd) {
    static const QMap<TechCommand, QString> names = {
                                                     {TechCommand::TS, "Опрос состояния"},
                                                     {TechCommand::TC, "Сброс"},
                                                     {TechCommand::VERS, "Запрос версии"},
                                                     {TechCommand::VOLUME, "Принять том исполняемого ПО"},
                                                     {TechCommand::CHECKSUM, "Выдать контрольную сумму"},
                                                     {TechCommand::PROGRAMM, "Обновить исполняемый файл ПО"},
                                                     {TechCommand::CLEAN, "Очистить временный файл ПО"},
                                                     {TechCommand::DROP, "Отброшенные пакеты ФУ"},
                                                     {TechCommand::PRBS_M2S, "Принять тестовую последовательность данных"},
                                                     {TechCommand::PRBS_S2M, "Выдать тестовую последовательность"},
                                                     {TechCommand::BER_T, "Коэффициент ошибок линии ТУ"},
                                                     {TechCommand::BER_F, "Коэффициент ошибок линии ФУ"},
                                                     { TechCommand::Factory_Number, "Выдать заводской номер устройства (0x1F)" }
                                                     };
    return names.value(cmd, "Неизвестная команда");
}

// TS

bool StatusCommand::parseResponseData(const QVector<QByteArray>& data,
                                      QString& outMessage,
                                      QVariant& outParsedData) const
{
    if (data.isEmpty()) {
        outMessage = "Ошибка: нет данных";
        return false;
    }

    const QByteArray& payload = data.first(); // весь UDP-пакет (заголовок уже отброшен)
    if (payload.size() < 3) {
        outMessage = "Ошибка: недостаточно данных (нет маски)";
        return false;
    }

    uint32_t mask = (static_cast<uint32_t>(static_cast<uint8_t>(payload[0])) << 16) |
                    (static_cast<uint32_t>(static_cast<uint8_t>(payload[1])) << 8) |
                    static_cast<uint32_t>(static_cast<uint8_t>(payload[2]));
    int offset = 3; // после маски

    QVector<QByteArray> blocks;
    // Максимальное количество двухбайтовых блоков – 24 (по числу битов в маске)
    for (int bit = 0; bit < 24; ++bit) {
        if (!(mask & (1 << bit))) continue;

        if (offset + 2 > payload.size()) {
            outMessage = QString("Ошибка: недостаточно данных для бита %1").arg(bit);
            return false;
        }

        QByteArray block = payload.mid(offset, 2);
        blocks.append(block);
        offset += 2;
    }

    QVariantMap result;
    result["mask"] = static_cast<uint>(mask); // сохраняем как uint (32 бита)
    QVariantList blocksList;
    for (const auto& block : blocks) {
        blocksList.append(block);
    }
    result["packets"] = blocksList;
    outParsedData = result;
    outMessage = QString("Статус получен: маска 0x%1, блоков %2")
                     .arg(mask, 6, 16, QChar('0'))
                     .arg(blocks.size());
    return true;
}

void StatusCommand::onDataReceived(CommandInterface* comm, const QVector<QByteArray>& data) const {
    if (!comm) {
        LOG_UI_ALERT("Command: StatusCommand::onDataReceived: comm is nullptr!");
        return;
    }

    // Сначала парсим данные
    QString message;
    QVariant parsedData;
    if (parseResponseData(data, message, parsedData)) {
        // Устанавливаем результат парсинга
        comm->setParseResult(true, message);
        comm->setParseData(parsedData);

        // Отправляем сырые данные для UI
        emit comm->statusDataReady(data);

        // Команда TS успешно выполнена только после получения всех пакетов
        // Завершение операции будет вызвано в communicationengine::completeOperation()
    } else {
        comm->setParseResult(false, "Ошибка парсинга статуса");
    }
}
// VERS
bool VersCommand::parseResponseData(const QVector<QByteArray>& data,
                                    QString& outMessage,
                                    QVariant& outParsedData) const {
    if (data.isEmpty()) {
        outMessage = "Ошибка: нет данных";
        return false;
    }

    // Берём первый пакет (единственный)
    const QByteArray& packet = data.first();
    if (packet.size() < 2) {
        outMessage = "Ошибка: недостаточно данных в пакете";
        return false;
    }

    // Извлекаем 2 байта данных (little-endian)
    uint32_t crc = 0;
    memcpy(&crc, packet.constData(), 2); // копируем 2 байта
    // Если нужно LE, используем qFromLittleEndian
    //crc = qFromLittleEndian<uint32_t>(crc);

    outMessage = QString("Версия ПО. CRC32: 0x%1").arg(crc, 8, 16, QChar('0'));

    QVariantMap extraData;
    extraData["crc32"] = crc;
    extraData["rawPackets"] = data.size();
    outParsedData = extraData;

    return true;
}

void VersCommand::onDataReceived(CommandInterface* comm, const QVector<QByteArray>& data) const {
    if (!comm) {
        LOG_UI_ALERT("Command: VersCommand::onDataReceived: comm is nullptr!");
        return;
    }

    QString message;
    QVariant parsedData;
    if (parseResponseData(data, message, parsedData)) {
        comm->setParseResult(true, message);
        comm->setParseData(parsedData);
    } else {
        comm->setParseResult(false, message);
    }

    emit comm->statusDataReady(data); // если нужно
}

// CHECKSUM
bool CheckSumCommand::parseResponseData(const QVector<QByteArray>& data,
                                        QString& outMessage,
                                        QVariant& outParsedData) const {
    if (data.isEmpty()) {
        outMessage = "Ошибка: нет данных";
        return false;
    }

    const QByteArray& packet = data.first();
    if (packet.size() < 2) {
        outMessage = "Ошибка: недостаточно данных в пакете";
        return false;
    }

    uint16_t checksum = 0;
    memcpy(&checksum, packet.constData(), 2);
    //checksum = qFromLittleEndian<uint16_t>(checksum);

    outMessage = QString("Контрольная сумма VOLUME: 0x%1").arg(checksum, 4, 16, QChar('0'));

    QVariantMap extraData;
    extraData["checksum"] = checksum;
    outParsedData = extraData;

    return true;
}

void CheckSumCommand::onDataReceived(CommandInterface* comm, const QVector<QByteArray>& data) const {
    if (!comm) {
        LOG_UI_ALERT("Command: CheckSumCommand::onDataReceived: comm is nullptr!");
        return;
    }

    QString message;
    QVariant parsedData;
    if (parseResponseData(data, message, parsedData)) {
        comm->setParseResult(true, message);
        comm->setParseData(parsedData);
    } else {
        comm->setParseResult(false, message);
    }
}

// DROP
bool DROPCommand::parseResponseData(const QVector<QByteArray>& data,
                                    QString& outMessage,
                                    QVariant& outParsedData) const {
    if (data.isEmpty()) {
        outMessage = "Ошибка: нет данных";
        return false;
    }

    const QByteArray& packet = data.first();
    if (packet.size() < 2) {
        outMessage = "Ошибка: недостаточно данных в пакете";
        return false;
    }

    uint16_t dropped = 0;
    memcpy(&dropped, packet.constData(), 2);
    //dropped = qFromLittleEndian<uint16_t>(dropped);

    outMessage = QString("Отброшенных пакетов ФУ: %1").arg(dropped);

    QVariantMap extraData;
    extraData["dropped"] = dropped;
    outParsedData = extraData;

    return true;
}

void DROPCommand::onDataReceived(CommandInterface* comm, const QVector<QByteArray>& data) const {
    if (!comm) {
        LOG_UI_ALERT("Command: DROPCommand::onDataReceived: comm is nullptr!");
        return;
    }

    QString message;
    QVariant parsedData;
    if (parseResponseData(data, message, parsedData)) {
        comm->setParseResult(true, message);
        comm->setParseData(parsedData);
    } else {
        comm->setParseResult(false, message);
    }
}

// ===== BER_TCommand =====
bool BER_TCommand::parseResponseData(const QVector<QByteArray>& data,
                                     QString& outMessage,
                                     QVariant& outParsedData) const {
    if (data.isEmpty()) {
        outMessage = "Ошибка: нет данных";
        return false;
    }

    const QByteArray& packet = data.first();
    if (packet.size() < 4) {
        outMessage = "Ошибка: недостаточно данных в пакете (ожидалось 4 байта)";
        return false;
    }

    uint32_t errors = 0;
    memcpy(&errors, packet.constData(), 4);
    //errors = qFromLittleEndian<uint32_t>(errors);

    float ber = errors / 1000000.0f;
    outMessage = QString("Коэффициент ошибок ТУ: %1 (ошибок: %2)").arg(ber, 0, 'f', 6).arg(errors);

    QVariantMap extraData;
    extraData["errors"] = errors;
    extraData["ber"] = ber;
    outParsedData = extraData;

    return true;
}

void BER_TCommand::onDataReceived(CommandInterface* comm, const QVector<QByteArray>& data) const {
    if (!comm) {
        LOG_UI_ALERT("Command: BER_TCommand::onDataReceived: comm is nullptr!");
        return;
    }

    QString message;
    QVariant parsedData;
    if (parseResponseData(data, message, parsedData)) {
        comm->setParseResult(true, message);
        comm->setParseData(parsedData);
    } else {
        comm->setParseResult(false, message);
    }
}

// ===== BER_FCommand =====
bool BER_FCommand::parseResponseData(const QVector<QByteArray>& data,
                                     QString& outMessage,
                                     QVariant& outParsedData) const {
    if (data.isEmpty()) {
        outMessage = "Ошибка: нет данных";
        return false;
    }

    const QByteArray& packet = data.first();
    if (packet.size() < 4) {
        outMessage = "Ошибка: недостаточно данных в пакете (ожидалось 4 байта)";
        return false;
    }

    uint32_t errors = 0;
    memcpy(&errors, packet.constData(), 4);
    errors = qFromLittleEndian<uint32_t>(errors);

    float ber = errors / 1000000.0f;
    outMessage = QString("Коэффициент ошибок ТУ: %1 (ошибок: %2)").arg(ber, 0, 'f', 6).arg(errors);

    QVariantMap extraData;
    extraData["errors"] = errors;
    extraData["ber"] = ber;
    outParsedData = extraData;

    return true;
}

void BER_FCommand::onDataReceived(CommandInterface* comm, const QVector<QByteArray>& data) const {
    if (!comm) {
        LOG_UI_ALERT("Command: BER_FCommand::onDataReceived: comm is nullptr!");
        return;
    }

    QString message;
    QVariant parsedData;
    if (parseResponseData(data, message, parsedData)) {
        comm->setParseResult(true, message);
        comm->setParseData(parsedData);
    } else {
        comm->setParseResult(false, message);
    }
}

// PRBS_M2S
static QVector<DataPacket> generateTestPackets() {
    QVector<DataPacket> packets;
    packets.reserve(512); // 512 пакетов * 2 байта = 1024 байта
    uint8_t lfsr = 0x01;
    for (int i = 0; i < 512; ++i) {
        DataPacket pkt;
        pkt.data[0] = lfsr;
        pkt.data[1] = lfsr ^ 0x55;
        packets.append(pkt);
        lfsr = (lfsr >> 1) | ((lfsr ^ (lfsr >> 1)) << 7);
    }
    return packets;
}

QByteArray PRBS_M2SCommand::buildRequest(uint16_t address) const {
    QVector<DataPacket> testPackets = generateTestPackets();
    QByteArray payload = PacketBuilder::buildPayloadFromPackets(testPackets);
    return PacketBuilder::createTURequestWithData(address, TechCommand::PRBS_M2S, payload);
}

void PRBS_M2SCommand::onOkReceived(CommandInterface* comm, uint16_t address) const {
    Q_UNUSED(address)
    QVector<DataPacket> testPackets = generateTestPackets();
    comm->notifySentPackets(testPackets);
    comm->completeCurrentOperation(true, "Тестовая последовательность отправлена (1024 байта)");
}

// ===== PRBS_S2MCommand =====
bool PRBS_S2MCommand::parseResponseData(const QVector<QByteArray>& data,
                                        QString& outMessage,
                                        QVariant& outParsedData) const
{
    // Простая проверка количества
    if (data.size() != expectedResponsePackets()) {
        outMessage = QString("Неверное количество пакетов: %1 (ожидалось %2)")
                         .arg(data.size())
                         .arg(expectedResponsePackets());
        return false;
    }

    outMessage = QString("Получено %1 пакетов тестовой последовательности").arg(data.size());

    QVariantMap extraData;
    extraData["packetCount"] = data.size();
    outParsedData = extraData;

    return true;
}

void PRBS_S2MCommand::onDataReceived(CommandInterface* comm, const QVector<QByteArray>& data) const
{
    if (!comm) return;
    if (data.isEmpty()) {
        comm->setParseResult(false, "Нет данных");
        comm->completeCurrentOperation(false, "Нет данных");
        return;
    }

    QVector<DataPacket> receivedPackets = PacketBuilder::extractDataPackets(data.first());
    comm->notifyReceivedPackets(receivedPackets);

    if (receivedPackets.size() != PPBConstants::TEST_PACKET_COUNT) {
        QString error = QString("Получено %1 из %2 пакетов")
                            .arg(receivedPackets.size())
                            .arg(PPBConstants::TEST_PACKET_COUNT);
        comm->setParseResult(false, error);
        comm->completeCurrentOperation(false, error);
        return;
    }

    QVariantMap extraData;
    extraData["packetCount"] = receivedPackets.size();
    extraData["parseErrors"] = 0;
    comm->setParseResult(true, QString("Получено %1 тестовых пакетов").arg(receivedPackets.size()));
    comm->setParseData(extraData);
    comm->completeCurrentOperation(true, "Тестовая последовательность получена");
}

// VOLUME
QByteArray VolumeCommand::buildRequest(uint16_t address) const
{
    QVector<DataPacket> programPackets;
    for (int i = 0; i < 256; ++i) {
        DataPacket packet;
        packet.data[0] = i & 0xFF;
        packet.data[1] = (i >> 8) & 0xFF;
        programPackets.append(packet);
    }
    QByteArray payload = PacketBuilder::buildPayloadFromPackets(programPackets);
    return PacketBuilder::createTURequestWithData(address, TechCommand::VOLUME, payload);
}

void VolumeCommand::onOkReceived(CommandInterface* comm, uint16_t address) const
{
    Q_UNUSED(address)
    comm->completeCurrentOperation(true, "Том ПО отправлен");
}

// ++++++++Factory_Number+++++++++++
bool Factory_Number::parseResponseData(const QVector<QByteArray>& data,
                                   QString& outMessage,
                                   QVariant& outParsedData) const
{
    if (data.isEmpty()) {
        outMessage = "Нет данных от команды 0x1F";
        return false;
    }

    const QByteArray& packet = data.first();
    // Предположим, что данные – это просто строка или число.
    // Здесь нужно распарсить согласно протоколу.
    // Например, если это 2 байта:
    if (packet.size() < 2) {
        outMessage = "Недостаточно данных";
        return false;
    }

    uint16_t value = 0;
    memcpy(&value, packet.constData(), 2);
    // При необходимости преобразовать порядок байт:
    // value = qFromBigEndian(value);

    outMessage = QString("Результат команды 0x1F: 0x%1 (%2)").arg(value, 4, 16, QChar('0')).arg(value);

    QVariantMap extra;
    extra["value"] = value;
    outParsedData = extra;
    return true;
}

void Factory_Number::onDataReceived(CommandInterface* comm, const QVector<QByteArray>& data) const
{
    if (!comm) return;

    QString msg;
    QVariant parsed;
    if (parseResponseData(data, msg, parsed)) {
        comm->setParseResult(true, msg);
        comm->setParseData(parsed);
    } else {
        comm->setParseResult(false, msg);
    }
    // Если нужно, можно также эмитировать сигнал для лога через comm
}
