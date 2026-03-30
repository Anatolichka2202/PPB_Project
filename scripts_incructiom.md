# Луа сценарии 

Сценарии на Lua позволяют автоматизировать управление ППБ (приемопередающими блоками). Вы можете создавать последовательности команд, выполнять циклические опросы, проверять работу оборудования и собирать диагностическую информацию.

Сценарии выполняются в отдельном потоке, не блокируя графический интерфейс. Если во время выполнения возникает ошибка, вы увидите сообщение в логе

## Требования к скрипту


- Файл должен иметь расширение .lua
- Кодировка UTF-8 (без BOM)
- Обязательно наличие глобальной функции main() – именно она будет запущена.
- Внутри main() можно вызывать предоставленные API-функции.


## Доступные функции

|Функция  |  Описание|Параметры|Возвращает|
|---------|----------|---------|----------|
|log(msg)|	Выводит сообщение в лог (и в статусную строку GUI)|	msg – строка|	ничего
|sleep(ms)|	Пауза в миллисекундах.|	ms – число|	ничего
|requestStatus(addr)|	Запрашивает текущее состояние ППБ (TS).|	addr – адрес (битовая маска)|	true – успех, false – ошибка
|sendTC(addr)|	Отправляет техническое управление (TC) с текущими настройками (мощности, флаги).|	addr – адрес|	true – успех, false – ошибка
|setFUReceive (addr, duration, dutyCycle)	|Устанавливает режим приёма ФУ.|	addr – адрес duration – длительность (мкс) dutyCycle – скважность (0–100)|	true – успех, false – ошибка
|setFUTransmit(addr, duration, dutyCycle)	|Устанавливает режим передачи ФУ.|	аналогично	|true – успех, false – ошибка
|requestVersion(addr)|	Запрашивает версию ПО ППБ.	|addr – адрес	|true – успех, false – ошибка
|requestChecksum(addr)|	Запрашивает контрольную сумму тома ПО.|	addr – адрес	|true – успех, false – ошибка
|requestDropped(addr)|	Запрашивает количество отброшенных пакетов ФУ.|	addr – адрес	|true – успех, false – ошибка
|requestBER_T(addr)|	Запрашивает коэффициент ошибок линии ТУ.|	addr – адрес	|true – успех, false – ошибка
|requestBER_F(addr)|	Запрашивает коэффициент ошибок линии ФУ.|	addr – адрес	|true – успех, false – ошибка
|requestFabricNumber(addr)|	Запрашивает заводской номер устройства.|	addr – адрес	|true – успех, false – ошибка
|startPRBS_M2S(addr)|	Запускает передачу тестовой последовательности (PRBS_M2S).|	addr – адрес	|true – успех, false – ошибка
|startPRBS_S2M(addr)|	Запускает приём тестовой последовательности (PRBS_S2M).|	addr – адрес	true – успех, false – ошибка



---
## 4. Адресация ППБ
Адреса задаются как битовая маска (16-битное число, big-endian). Каждому ППБ соответствует один бит:

|ППБ	|Адрес (бит)	|Маска (hex) |
|---    |---           |---|
|1	|1 << 0	|0x0001|
|2	|1 << 1	|0x0002|
|3	|1 << 2	|0x0004|
|…	|…	|…  |
|16	|1 << 15	|0x8000| 

### Для групповых операций можно использовать маску, объединяющую несколько битов. Например, 0x000F = ППБ1–4.

## Примеры

### Базовый тест одного ппб
```
function main()
    log("=== Start basic test ===")
    local addr = 0x0001
    if not requestStatus(addr) then
        log("Failed to get status")
        return false
    end
    sleep(500)
    if not requestVersion(addr) then
        log("Failed to get version")
        return false
    end
    sleep(200)
    if not sendTC(addr) then
        log("Failed to send TC")
        return false
    end
    log("Basic test passed")
    return true
end
```
### Циклический опрос нескольких ппб
```
local addresses = {0x0001, 0x0002, 0x0004, 0x0008}
local cycles = 10
function main()
    for i = 1, cycles do
        for _, addr in ipairs(addresses) do
            if not requestStatus(addr) then
                log("Cycle " .. i .. ": PPB " .. addr .. " failed")
                return false
            end
            sleep(100)
        end
        if i % 2 == 0 then
            log("Completed " .. i .. " cycles")
        end
    end
    log("All cycles completed")
    return true
end
```
### Проврка PRBS & BER
```
function main()
    local addr = 0x0001
    log("Starting PRBS_M2S")
    if not startPRBS_M2S(addr) then
        log("PRBS_M2S failed")
        return false
    end
    sleep(2000)
    log("Starting PRBS_S2M")
    if not startPRBS_S2M(addr) then
        log("PRBS_S2M failed")
        return false
    end
    sleep(2000)
    log("Requesting BER_T")
    if not requestBER_T(addr) then
        log("BER_T failed")
        return false
    end
    log("Requesting BER_F")
    if not requestBER_F(addr) then
        log("BER_F failed")
        return false
    end
    log("PRBS/BER test completed")
    return true
end
```

## Рекомендации по написанию
- Всегда проверяйте возвращаемые значения – если команда не удалась, лучше прервать выполнение и вернуть false.

- Добавляйте паузы (sleep) между командами, чтобы оборудование успело обработать запрос. Минимальная пауза – 100–200 мс, для длительных операций (PRBS) – несколько секунд.

- Используйте log() для отладки и фиксации промежуточных результатов.

- Имена переменных: локальные переменные (local) не видны вне функции, но это безопасно. Глобальные переменные создавать не рекомендуется, чтобы не мешать другим скриптам.

- Обработка ошибок: если скрипт вернёт false, это будет зафиксировано как ошибка. При выполнении через GUI вы увидите сообщение в логе и окне.

##  Загрузка и запуск в графическом интерфейсе
### Откройте вкладку Сценарии (Scenario)
- #### Нажмите кнопку Загрузить и выберите файл .lua

- #### Нажмите Выполнить. В процессе выполнения вы будете видеть сообщения в логе и статусной строке.

- #### При необходимости используйте кнопку Стоп для досрочного прерывания.

## Запуск скрипта без графического интерфейса (headless)
### Программа может быть запущена из командной строки с параметрами:

 
`PPB_GUI_Sandbox --headless --script <путь_к_скрипту>`

`--headless – отключает GUI, все сообщения выводятся в консоль.`

`--script – указывает файл скрипта. Если опущен, будет выполнен встроенный autotest.lua.`


## Пример:

`bash` \
`PPB_GUI_Sandbox --headless --script C:\myscript.lua`

### Код возврата:

### 0 – сценарий успешно выполнен (функция main вернула true) 

### 1 – ошибка загрузки или выполнения.

### Этот режим удобен для автоматического тестирования и интеграции

## Встроенные скрипты

> ### В исполняемый файл уже встроены три стандартных скрипта:
>
> - autotest.lua – базовый функциональный тест. 
>
> - stress.lua – нагрузочный тест (циклический опрос).
> 
> - loadtest.lua – проверка PRBS и BER.

#### Чтобы запустить их в headless-режиме:

`bash` \
`PPB_GUI_Sandbox --headless --script :/scenario/scripts/autotest.lua` 


