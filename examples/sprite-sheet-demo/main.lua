-- Milestone 3C: 4-frame PNG sprite sheet, 48x32 per frame.
-- Arrows/WASD/controller D-pad move the bee. Facing reverses with travel.
-- Animation timing belongs to the game, not the graphics backend.
local sheet, x, y, frame, elapsed, facing_left
local frame_w, frame_h = 48, 32
local frame_count = 4

function init()
    sheet = sugarbee.texture_load("assets/bee-sheet.png")
    x, y = 160, 170
    frame, elapsed, facing_left = 0, 0, false
end

function update(dt)
    elapsed = elapsed + dt
    frame = math.floor(elapsed * 8) % frame_count  -- eight animation frames/sec

    local dx, dy = 0, 0
    if sugarbee.btn("left") then dx = dx - 1 end
    if sugarbee.btn("right") then dx = dx + 1 end
    if sugarbee.btn("up") then dy = dy - 1 end
    if sugarbee.btn("down") then dy = dy + 1 end
    if dx < 0 then facing_left = true end
    if dx > 0 then facing_left = false end
    x = math.max(0, math.min(1280 - 144, x + dx * 180 * dt))
    y = math.max(65, math.min(720 - 96, y + dy * 180 * dt))
end

function draw()
    sugarbee.cls(8, 12, 32)
    -- Source changes each frame; the same source is scaled 3x with crisp pixels.
    sugarbee.sprite_region(sheet, math.floor(x), math.floor(y),
                           frame * frame_w, 0, frame_w, frame_h,
                           144, 96, facing_left, false)

    -- Static samples of all four sheet cells and both flip directions.
    for f = 0, frame_count - 1 do
        sugarbee.sprite_region(sheet, 70 + f * 145, 485,
                               f * frame_w, 0, frame_w, frame_h,
                               96, 64)
    end
    sugarbee.sprite_region(sheet, 720, 485, 0, 0, frame_w, frame_h,
                           96, 64, true, false)
    sugarbee.sprite_region(sheet, 915, 485, 0, 0, frame_w, frame_h,
                           96, 64, false, true)
    sugarbee.print("SUGARBEE SPRITE SHEET", 40, 30, 255, 255, 255, 2)
    sugarbee.print("MOVE WITH ARROWS, WASD OR GAMEPAD", 40, 65)
    sugarbee.print("FRAMES: 0  1  2  3", 80, 580)
    sugarbee.print("FLIP X", 734, 580)
    sugarbee.print("FLIP Y", 928, 580)
end
