-- Geometry follows the declared canvas; no physical window dimensions here.
local w, h = sugarbee.width(), sugarbee.height()
local elapsed = 0
function init() end
function update(dt)
    elapsed = elapsed + dt
    if sugarbee.btnp('b') then error('Intentional canvas-demo Guru check') end
end
function draw()
    sugarbee.cls(18, 25, 42)
    sugarbee.rect(0, 0, w, h, 255, 255, 255, false)
    sugarbee.print(w .. ' x ' .. h .. ' canvas', 8, 8, 255, 220, 80, 1)
    sugarbee.print('ENTER: pause / K: Guru', 8, 24, 255, 255, 255, 1)
    -- Alternating one-pixel bars should have uniform widths at integer scales.
    for x = 8, 71 do
        local c = x % 2 == 0 and 255 or 0
        sugarbee.rect(x, 46, 1, 24, c, c, c, true)
    end
    sugarbee.circle(104, 58, 12, 120, 220, 255)
    sugarbee.clip(8, 88, w - 16, h - 112)
    sugarbee.camera(math.floor(20 * math.sin(elapsed)), 0)
    sugarbee.line(-2147483648, 100, 2147483647, 100, 80, 140, 220)
    sugarbee.rect(math.floor(w / 2) - 8, 110, 16, 16, 255, 160, 70, true)
    sugarbee.camera_reset()
    sugarbee.clip_reset()
    sugarbee.print('Clipped moving square', 8, h - 16, 255, 255, 255, 1)
end
function shutdown() end
