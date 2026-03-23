-- loadtest.lua
-- Тест PRBS и BER (одного ППБ)
function main()
    log("=== Starting PRBS/BER test ===")
    local addr = 0x0001

    -- Запуск передачи PRBS_M2S (отправка тестовых данных)
    if not startPRBS_M2S(addr) then
        log("FAIL: PRBS_M2S start")
        return false
    end
    log("PRBS_M2S started, waiting 2 seconds...")
    sleep(2000)

    -- Запуск приёма PRBS_S2M (получение тестовых данных)
    if not startPRBS_S2M(addr) then
        log("FAIL: PRBS_S2M start")
        return false
    end
    log("PRBS_S2M started, waiting 2 seconds...")
    sleep(2000)

    -- Запрос BER ТУ
    if not requestBER_T(addr) then
        log("FAIL: BER_T request")
        return false
    end
    sleep(200)

    -- Запрос BER ФУ
    if not requestBER_F(addr) then
        log("FAIL: BER_F request")
        return false
    end

    log("PRBS/BER test passed")
    return true
end
