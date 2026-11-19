local w, h = sugarbee.width(), sugarbee.height()
local samples = {'Paused', 'Two\nlines', ' A  ', 'End\n', '\n'}
local selected = 1
function init() end
function update()
    if sugarbee.btnp('a') then selected = selected % #samples + 1 end
end
local function panel(text, scale, top, bottom)
    local tw, th = sugarbee.measure_text(text, scale)
    local x = math.floor((w - tw) / 2)
    local y = top + math.floor((bottom - top - th) / 2)
    -- Border is just outside the measured cells; spaces/blank lines stay inside.
    sugarbee.rect(x - 1, y - 1, tw + 2, th + 2, 100, 200, 255, false)
    sugarbee.print(text, x, y, 255, 255, 255, scale)
    local label = tw .. ' x ' .. th .. ' pixels / scale ' .. scale
    local lw = sugarbee.measure_text(label)
    sugarbee.print(label, math.floor((w - lw) / 2), bottom + 4, 255, 215, 80, 1)
end
function draw()
    sugarbee.cls(18, 25, 42)
    sugarbee.print('A/J: cycle measured text', 8, 8, 255, 255, 255, 1)
    panel(samples[selected], 1, 30, 84)
    panel(samples[selected], 2, 112, 194)
    local caption = selected .. '/' .. #samples .. '  ENTER: pause'
    local cw = sugarbee.measure_text(caption)
    sugarbee.print(caption, math.floor((w - cw) / 2), h - 16, 255, 255, 255, 1)
end
function shutdown() end
