function main()
    -- Логирование (выводится в статусную строку GUI)
    log("Сценарий начат")

    -- Адрес ППБ (битовая маска: 1 << индекс, где индекс 0..15)
    local addr = 0x0001  -- ППБ1

    -- 1. Запрос статуса (ждёт ответа до 10 сек)
    log("Запрашиваю статус ППБ " .. addr)
    local ok = requestStatus(addr)
    if not ok then
        log("Ошибка запроса статуса")
        return  -- завершаем сценарий
    end
    log("Статус получен")

    -- Пауза 1 секунда
    sleep(1000)

    -- 2. Установка параметров TC и отправка
    -- Перед отправкой TC можно задать мощности (в настройках пользователя)
    -- Для этого в контроллере есть методы setPowerSetting и др., но они не в Lua API.
    -- В Lua API только команды, которые отправляются на ППБ.
    -- Пример: sendTC использует текущие настройки из контроллера (установленные ранее через GUI).
    log("Отправляю TC")
    ok = sendTC(addr)
    if not ok then
        log("Ошибка TC")
        return
    end

    -- 3. ФУ-команды (приём и передача)
    log("ФУ приём")
    ok = setFUReceive(addr, 100, 50)  -- длительность 100, скважность 50
    if not ok then
        log("Ошибка ФУ приёма")
        return
    end

    sleep(500)

    log("ФУ передача")
    ok = setFUTransmit(addr, 100, 50)
    if not ok then
        log("Ошибка ФУ передачи")
        return
    end

    -- 4. Запрос версии
    log("Запрос версии")
    ok = requestVersion(addr)
    if ok then
        log("Версия запрошена успешно")
    end

    -- 5. Запрос контрольной суммы
    log("Запрос контрольной суммы")
    ok = requestChecksum(addr)
    if ok then
        log("Контр. сумма запрошена")
    end

    -- 6. Запрос отброшенных пакетов
    log("Запрос отброшенных пакетов")
    ok = requestDropped(addr)
    if ok then
        log("DROP получен")
    end

    -- 7. Запрос BER ТУ
    log("Запрос BER ТУ")
    ok = requestBER_T(addr)
    if ok then
        log("BER ТУ получен")
    end

    -- 8. Запрос заводского номера
    log("Запрос заводского номера")
    ok = requestFabricNumber(addr)
    if ok then
        log("Заводской номер получен")
    end

    -- 9. Тестовые последовательности
    log("Запуск PRBS_M2S")
    ok = startPRBS_M2S(addr)
    if ok then
        log("PRBS_M2S запущен")
    end

    sleep(2000)

    log("Запуск PRBS_S2M")
    ok = startPRBS_S2M(addr)
    if ok then
        log("PRBS_S2M запущен")
    end

    log("Сценарий завершён")
end