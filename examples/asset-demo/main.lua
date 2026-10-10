-- Milestone 3B: sprite loaded from the cartridge ZIP, not generated in Lua.
local bee, x, y

function init()
    bee = sugarbee.texture_load("assets/bee.png")
    x, y = 100, 100
end

function update(dt)
    local step = 140 * dt
    if sugarbee.btn("left") then x = x - step end
    if sugarbee.btn("right") then x = x + step end
    if sugarbee.btn("up") then y = y - step end
    if sugarbee.btn("down") then y = y + step end
end

function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.sprite(bee, math.floor(x), math.floor(y))
    sugarbee.print("SUGARBEE ASSETS", 40, 35, 255, 255, 255, 2)
end
