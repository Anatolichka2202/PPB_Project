-- stress.lua
-- Нагрузочный тест: циклический опрос нескольких ППБ
local addresses = {0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080}
local cycles = 100   -- количество циклов

function main()
    log("=== Starting stress test ===")
    log("Cycles: " .. cycles .. ", addresses: " .. #addresses)

    for i = 1, cycles do
        for _, addr in ipairs(addresses) do
            if not requestStatus(addr) then
                log(string.format("Cycle %d, address 0x%04X: FAIL", i, addr))
                return false
            end
            sleep(50)   -- небольшая пауза между опросами
        end
        if i % 10 == 0 then
            log("Completed " .. i .. " cycles")
        end
    end

    log("Stress test passed")
    return true
end
