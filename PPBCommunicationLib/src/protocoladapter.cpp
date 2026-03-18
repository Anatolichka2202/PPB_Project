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

    if (data.size() == 2) {
        event.address = 0; // адрес бриджа условно
        event.type = ProtocolEvent::Data; // используем тип Data
        event.payload = data;
        return true;
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

QByteArray ProtocolAdapter::buildFURequest(uint16_t address, uint8_t cmd, uint8_t period, const uint8_t fuData[3]) {
    return PacketBuilder::createFURequest(address, cmd, period, fuData);
}

QVector<DataPacket> ProtocolAdapter::extractDataPackets(const QByteArray& payload) {
    return PacketBuilder::extractDataPackets(payload);
}

bool ProtocolAdapter::parseTUResponse(const QByteArray& data, ProtocolEvent& event) {
    event = ProtocolEvent();
    if (data.size() < 3) return false;

    uint16_t address = qFromBigEndian(*reinterpret_cast<const uint16_t*>(data.constData()));
    uint8_t status = static_cast<uint8_t>(data[2]);

    event.address = address;
    event.status = status;

    if (status != 0) {
        event.type = ProtocolEvent::Error;
        return true;
    }

    // Статус 0 – OK
    if (data.size() == 3) {
        event.type = ProtocolEvent::Ok;                // старый 3-байтовый OK
    } else if (data.size() == 4) {
        event.type = ProtocolEvent::Ok;                // новый 4-байтовый OK (команда игнорируется)
    } else {
        event.type = ProtocolEvent::Data;              // есть данные
        event.payload = data.mid(3);                   // данные после 3-байтового заголовка
    }
    return true;
}

bool ProtocolAdapter::parseBridgeResponse(const QByteArray& data, ProtocolEvent& event) {
    event = ProtocolEvent();

    // Проверка на "ERR" (3 байта)
    if (data.size() == 3 && data == QByteArray("ERR")) {
        event.address = 0; // адрес не определён
        event.status = 0;  // ошибка
        event.type = ProtocolEvent::BridgeResponse;
        // Можно сохранить сырые данные для логирования
        event.payload = data;
        return true;
    }

    // Обычный 4-байтовый ответ
    if (data.size() != 4) return false;

    BridgeResponse resp;
    memcpy(&resp, data.constData(), 4);
    resp.address = qFromBigEndian(resp.address);

    // Дополнительная проверка: команда должна быть 0 или 1
    if (resp.command != 0 && resp.command != 1) return false;

    event.address = resp.address;
    event.status = resp.status; // 1 – OK, 0 – ошибка
    event.type = ProtocolEvent::BridgeResponse;
    event.payload = data; // можно сохранить для отладки
    return true;
}
