-- Cases: runaway, normal, caught_error
local test_case = "runaway"
local frames = 0
local total = 0

function init()
    print("Execution limit test: " .. test_case)
end

local function runaway()
    while true do end
end

function update(dt)
    if test_case == "runaway" then
        runaway()
    elseif test_case == "caught_error" then
        pcall(runaway)
    elseif test_case == "normal" then
        -- Each call fits; cumulative work exceeds the per-call allowance.
        for i = 1, 50000 do total = total + i end
        frames = frames + 1
        if frames == 120 then print("Execution limit test: 120 updates completed") end
    else
        error("Unknown execution limit test case: " .. test_case)
    end
end

function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.print("EXECUTION LIMIT TEST", 40, 40)
    sugarbee.print("NORMAL CASE: START TO OPEN EXIT MENU", 40, 80)
end

function shutdown()
    print("Execution limit test: shutdown entered")
end
