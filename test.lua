function main()
    log("Starting test")
    local ok = requestStatusSync(0x0001)
    if ok then
        log("Status OK")
    else
        log("Status failed")
    end
end
