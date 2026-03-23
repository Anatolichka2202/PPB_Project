-- autotest.lua
-- Базовый функциональный тест одного ППБ (адрес 0x0001)
function main()
    log("=== Starting functional test ===")
    local addr = 0x0001

    -- 1. Запрос статуса
    if not requestStatus(addr) then
        log("FAIL: Status request")
        return false
    end
    sleep(500)

    -- 2. Запрос версии
    if not requestVersion(addr) then
        log("FAIL: Version request")
        return false
    end
    sleep(200)

    -- 3. Запрос контрольной суммы
    if not requestChecksum(addr) then
        log("FAIL: Checksum request")
        return false
    end
    sleep(200)

    -- 4. Отправка TC (с текущими настройками)
    if not sendTC(addr) then
        log("FAIL: TC send")
        return false
    end
    sleep(200)

    -- 5. Запрос отброшенных пакетов
    if not requestDropped(addr) then
        log("FAIL: Dropped packets request")
        return false
    end
    sleep(200)

    -- 6. Запрос BER ТУ
    if not requestBER_T(addr) then
        log("FAIL: BER_T request")
        return false
    end
    sleep(200)

    -- 7. Запрос заводского номера
    if not requestFabricNumber(addr) then
        log("FAIL: Fabric number request")
        return false
    end

    log("Functional test passed")
    return true
end
