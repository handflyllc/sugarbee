-- Cases: runaway, normal, caught_error, main_runaway, init_runaway,
-- main_caught_error, init_caught_error, startup_normal,
-- draw_runaway, draw_caught_error, draw_normal, shutdown_runaway,
-- shutdown_caught_error, shutdown_normal, shutdown_error, shutdown_absent
local test_case = "runaway"
local frames = 0
local draws = 0
local total = 0

local function runaway()
    while true do end
end

local function startup_work()
    -- Larger than the update allowance, but fits a startup allowance.
    local sum = 0
    for i = 1, 3000000 do sum = sum + i end
    return sum
end

if test_case == "main_runaway" then
    runaway()
elseif test_case == "main_caught_error" then
    pcall(runaway)
elseif test_case == "startup_normal" then
    total = startup_work()
    print("Execution limit test: main.lua completed")
end

function init()
    print("Execution limit test: " .. test_case)
    if test_case == "init_runaway" then
        runaway()
    elseif test_case == "init_caught_error" then
        pcall(runaway)
    elseif test_case == "startup_normal" then
        total = total + startup_work()
        print("Execution limit test: init completed")
    end
end

function update(dt)
    if test_case == "runaway" then
        runaway()
    elseif test_case == "caught_error" then
        pcall(runaway)
    elseif test_case == "normal" or test_case == "startup_normal" then
        -- Each call fits; cumulative work exceeds the per-call allowance.
        for i = 1, 50000 do total = total + i end
        frames = frames + 1
        if frames == 120 then print("Execution limit test: 120 updates completed") end
    elseif test_case == "draw_runaway" or test_case == "draw_caught_error" or
           test_case == "draw_normal" or test_case == "shutdown_runaway" or
           test_case == "shutdown_caught_error" or test_case == "shutdown_normal" or
           test_case == "shutdown_error" or test_case == "shutdown_absent" then
        -- Leave update healthy so drawing and normal exit can run.
    else
        error("Unknown execution limit test case: " .. test_case)
    end
end

function draw()
    if test_case == "draw_runaway" then
        runaway()
    elseif test_case == "draw_caught_error" then
        pcall(runaway)
    elseif test_case == "draw_normal" then
        -- Each draw fits; repeated draws exceed a single-call allowance.
        for i = 1, 50000 do total = total + i end
        draws = draws + 1
        if draws == 120 then print("Execution limit test: 120 draws completed") end
    end

    sugarbee.cls(8, 12, 32)
    sugarbee.print("EXECUTION LIMIT TEST", 40, 40)
    sugarbee.print("NORMAL CASE: START TO OPEN EXIT MENU", 40, 80)
end

function shutdown()
    print("Execution limit test: shutdown entered")
    if test_case == "shutdown_runaway" then
        runaway()
    elseif test_case == "shutdown_caught_error" then
        pcall(runaway)
    elseif test_case == "shutdown_normal" then
        for i = 1, 50000 do total = total + i end
    elseif test_case == "shutdown_error" then
        error("Intentional shutdown failure")
    end
    print("Execution limit test: shutdown completed")
end

if test_case == "shutdown_absent" then
    shutdown = nil
end
