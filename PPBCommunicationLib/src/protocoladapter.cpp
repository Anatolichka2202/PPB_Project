#include "protocoladapter.h"
#include "../comm_dependencies.h"  // для логирования (если нужно)
#include <cstring>
#include <QtEndian>
bool ProtocolAdapter::parseIncomingPacket(const QByteArray& data, ProtocolEvent& event) {
    event = ProtocolEvent();
    event.type = ProtocolEvent::Unknown;

    // Сначала проверяем BridgeResponse (фиксированный размер 4 байта)
    if (data.size() == sizeof(BridgeResponse)) {
        BridgeResponse response;
        std::memcpy(&response, data.constData(), sizeof(response));
        // Можно добавить дополнительную проверку, что response.command в допустимых пределах
        // Например, если command == 0 или 1, то это скорее всего бридж
        // Но для простоты пока считаем любой пакет размером 4 байта ответом бриджа
        event.address = response.address;
        event.status = response.status; // 1 - OK, 0 - ошибка
        event.type = ProtocolEvent::BridgeResponse;
        return true;
    }

    // Затем проверяем TU ответ (минимум 3 байта)
    if (data.size() >= static_cast<int>(sizeof(TUResponseHeader))) {
        TUResponseHeader header;
        std::memcpy(&header, data.constData(), sizeof(header));
         header.address = qFromBigEndian(header.address);
        if (header.address != 0) {
            event.address = header.address;
            event.status = header.status;
            if (header.status == 0) {
                if (data.size() > sizeof(header)) {
                    event.type = ProtocolEvent::Data;
                    event.payload = data.mid(sizeof(header));
                } else {
                    event.type = ProtocolEvent::Ok;
                }
            } else {
                event.type = ProtocolEvent::Error;
            }
            return true;
        }
    }

    return false;
}

QByteArray ProtocolAdapter::buildRequest(uint16_t address, TechCommand cmd, const QByteArray& payload) {
    if (payload.isEmpty()) {
        return PacketBuilder::createTURequest(address, cmd);
    } else {
        return PacketBuilder::createTURequestWithData(address, cmd, payload);
    }
}

QByteArray ProtocolAdapter::buildFURequest(uint16_t address, uint8_t period, const uint8_t fuData[3]) {
    return PacketBuilder::createFURequest(address, period, fuData);
}

QVector<DataPacket> ProtocolAdapter::extractDataPackets(const QByteArray& payload) {
    return PacketBuilder::extractDataPackets(payload);
}
