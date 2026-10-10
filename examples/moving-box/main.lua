-- SugarBee input milestone demo; update() does not yet receive delta time.
local x = 100
local y = 200

function init()
    print("SugarBee moving-box demo loaded")
end

function update()
    if sugarbee.btn("left")  then x = x - 3 end
    if sugarbee.btn("right") then x = x + 3 end
    if sugarbee.btn("up")    then y = y - 3 end
    if sugarbee.btn("down")  then y = y + 3 end

    if sugarbee.btnp("a") then
        print("A pressed")
    end
    -- Keep our box inside the default 1280x720 viewport for this demo.
    x = math.max(0, math.min(1248, x))
    y = math.max(0, math.min(688, y))
end

function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.rect(x, y, 32, 32, 240, 180, 50, true)
    sugarbee.print("MOVE: ARROWS/WASD OR GAMEPAD D-PAD", 30, 30, 255, 255, 255, 2)
    sugarbee.print("A: J KEY OR GAMEPAD A", 30, 60, 255, 255, 255, 2)
end
