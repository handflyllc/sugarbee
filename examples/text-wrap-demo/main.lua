local wrap = require('ui.textwrap')
local samples = {
    'SugarBee wraps words to fit the available width. Layout is calculated only when the text or its settings change.',
    'First paragraph.\n\nA blank line above; a trailing newline below.\n',
    'A_very_long_word_without_any_spaces_to_test_splitting_at_cell_boundaries.',
    '   Multiple   spaces   collapse.   ',
    '',
}
local selected, scale, narrow = 1, 1, false
local lines, height, width
local function layout()
    width = math.floor((sugarbee.width() - 32) / (narrow and 2 or 1))
    lines, height = wrap.wrap(samples[selected], width, scale)
end
function init() layout() end
function update()
    local changed = false
    if sugarbee.btnp('a') then selected = selected % #samples + 1; changed = true end
    if sugarbee.btnp('b') then narrow = not narrow; changed = true end
    if sugarbee.btnp('x') then scale = 3 - scale; changed = true end
    if changed then layout() end
end
function draw()
    sugarbee.cls(18, 25, 42)
    sugarbee.print('J: sample  K: width  U: scale', 8, 8, 255, 255, 255)
    sugarbee.print('Sample ' .. selected .. ' / scale ' .. scale .. ' / height ' .. height, 8, 24, 255, 215, 80)
    sugarbee.rect(15, 47, width + 2, math.max(height, 8) + 2, 100, 200, 255, false)
    for i, line in ipairs(lines) do
        sugarbee.print(line, 16, 48 + (i - 1) * 10 * scale, 255, 255, 255, scale)
    end
end
function shutdown() end
