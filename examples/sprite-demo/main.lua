-- Phase 3A: generated RGBA8 sprite, no external image files yet.
-- Arrow keys/WASD or controller D-pad move the little striped bee.
local bee, x, y

local function make_bee()
    local width, height = 48, 32
    local bytes = {}
    for py = 0, height - 1 do
        for px = 0, width - 1 do
            local r, g, b, a = 0, 0, 0, 0
            local px0, py0 = math.floor(px / 2), math.floor(py / 2)
            -- A simple pixel-art bee, with transparent pixels around it.
            if px0 >= 3 and px0 <= 20 and py0 >= 4 and py0 <= 13 then
                r, g, b, a = 247, 196, 55, 255
                if (px0 >= 8 and px0 <= 10) or (px0 >= 15 and px0 <= 17) then
                    r, g, b = 42, 38, 56
                end
                if px0 == 19 and py0 == 6 then r, g, b = 15, 15, 15 end
            elseif py0 >= 1 and py0 <= 4 and
                   ((px0 >= 6 and px0 <= 10) or (px0 >= 13 and px0 <= 17)) then
                r, g, b, a = 179, 227, 241, 220
            end
            bytes[#bytes + 1] = string.char(r, g, b, a)
        end
    end
    return sugarbee.texture_rgba(width, height, table.concat(bytes))
end

function init()
    bee = make_bee()
    x, y = 100, 100
end

function update(dt)
    local speed = 140 * dt
    if sugarbee.btn("left") then x = x - speed end
    if sugarbee.btn("right") then x = x + speed end
    if sugarbee.btn("up") then y = y - speed end
    if sugarbee.btn("down") then y = y + speed end
end

function draw()
    sugarbee.cls(8, 12, 32)
    sugarbee.sprite(bee, math.floor(x), math.floor(y))
    sugarbee.print("SUGARBEE SPRITES", 40, 35, 255, 255, 255, 2)
end
