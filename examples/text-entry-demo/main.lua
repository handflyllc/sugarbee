local textline = require('ui.textline')
local editor, typing, history, message, x
function init()
    editor, typing, history, message, x = textline.new(), false, {}, '', 40
end
function update(dt)
    if typing then
        local events, status = sugarbee.text_events()
        local submitted, err = editor:apply(events, status)
        if err then message = 'Input rejected: ' .. err .. '. Line unchanged; try again.'
        elseif #events > 0 then message = '' end
        if submitted ~= nil then
            history[#history + 1] = submitted
            if #history > 6 then table.remove(history, 1) end
            sugarbee.text_end()
            typing = false
            message = 'Submitted. Movement enabled. J/A starts a new line.'
        end
    else
        if sugarbee.btn('left') then x = math.max(16, x - 100 * dt) end
        if sugarbee.btn('right') then x = math.min(sugarbee.width() - 32, x + 100 * dt) end
        if sugarbee.btnp('a') then
            editor = textline.new()
            sugarbee.text_begin()
            typing = true
            message = 'Type ASCII text. Enter submits; Escape pauses.'
        end
    end
end
function draw()
    sugarbee.cls(18, 25, 42)
    sugarbee.print('TEXT ENTRY / J or pad A: begin / Escape or pad Start: pause', 16, 16, 255, 255, 255)
    sugarbee.print(typing and 'TYPING: movement disabled by this game' or 'MOVEMENT: Left/Right or A/D', 16, 36, 255, 215, 80)
    sugarbee.rect(math.floor(x), 60, 16, 16, 255, 215, 80, true)
    sugarbee.print('History (last six submissions; long lines abbreviated):', 16, 100, 180, 200, 255)
    local columns = math.floor((sugarbee.width() - 48) / 8)
    for i, text in ipairs(history) do
        if #text > columns then text = text:sub(1, columns - 3) .. '...' end
        sugarbee.print(text == '' and '(empty)' or text, 16, 112 + (i - 1) * 12, 255, 255, 255)
    end
    -- Horizontal scrolling is game-owned and independent of the text buffer.
    local first = math.max(1, editor.cursor - columns + 2)
    local visible = editor.text:sub(first, first + columns - 1)
    sugarbee.rect(15, 209, columns * 8 + 2, 14, 100, 200, 255, false)
    sugarbee.print(visible, 16, 212, 255, 255, 255)
    if typing then
        local caret = editor.cursor - first + 1
        sugarbee.line(16 + caret * 8, 211, 16 + caret * 8, 220, 255, 215, 80)
    end
    sugarbee.print(#editor.text .. '/256 bytes / cursor ' .. editor.cursor, 16, 236, 180, 200, 255)
    sugarbee.print(message, 16, 254, 255, 215, 80)
end
function shutdown() end
