local model = require('app')
local layout = require('ui.layout')
local textwrap = require('ui.textwrap')
local app, panes, output, status_lines
local function refresh()
    panes = layout.panes(app.view, sugarbee.width(), sugarbee.height())
    output = {}
    for _, pane in ipairs(panes) do
        if pane.name == 'console' then
            for _, entry in ipairs(app.history) do
                local lines = textwrap.wrap(entry, pane.w - 32)
                for _, line in ipairs(lines) do output[#output + 1] = line end
            end
        elseif pane.name == 'status' then
            status_lines = textwrap.wrap('Station: ' .. (app.position + 1) .. ' of 5\nMoves: ' .. app.moves ..
                '\n\nCommands:\nview all\nview scene\nview console\nview status\nmove left\nmove right\nlook\nhelp' ..
                '\n\nTyping stays active in every view. Use view all to restore the split.', pane.w - 32)
        end
    end
end
function init()
    app = model.new()
    refresh()
    sugarbee.text_begin()
end
function update(dt)
    local events, status = sugarbee.text_events()
    if app:input(events, status) then refresh() end
end
local function text(s, x, y, r, g, b)
    sugarbee.print(s, x, y, r or 220, g or 230, b or 240)
end
local function command_strip(pane)
    local x, y, width = pane.x + 16, pane.y + pane.h - 54, pane.w - 32
    sugarbee.rect(x - 4, y - 4, width + 8, 46, 15, 22, 32, true)
    if pane.name ~= 'console' then
        -- Keep command feedback visible even when the output pane is hidden.
        text(app.history[#app.history], x, y - 18)
    end
    text('> ', x, y, 255, 215, 80)
    local columns = math.floor((width - 16) / 8)
    local first = math.max(1, app.editor.cursor - columns + 2)
    text(app.editor.text:sub(first, first + columns - 1), x + 16, y)
    local caret = app.editor.cursor - first + 1
    sugarbee.line(x + 16 + caret * 8, y - 1, x + 16 + caret * 8, y + 8, 255, 215, 80)
    text(app.message ~= '' and app.message or
         ('Enter: submit / Escape: pause / ' .. #app.editor.text .. '/256'), x, y + 16, 255, 215, 80)
end
function draw()
    sugarbee.cls(12, 18, 28)
    for _, pane in ipairs(panes) do
        sugarbee.clip(pane.x, pane.y, pane.w, pane.h)
        sugarbee.rect(pane.x, pane.y, pane.w, pane.h, 30, 42, 56, true)
        sugarbee.rect(pane.x, pane.y, pane.w, pane.h, 100, 140, 170, false)
        local x, y = pane.x + 16, pane.y + 16
        text(pane.name:upper() .. ' / view ' .. app.view, x, y, 255, 215, 80)
        if pane.name == 'scene' then
            text('Five stations / move left / move right', x, y + 24)
            local spacing = math.floor((pane.w - 64) / 5)
            local baseline = pane.y + math.floor(pane.h / 2)
            for station = 0, 4 do
                local sx = pane.x + 32 + station * spacing
                sugarbee.rect(sx, baseline, 40, 24, 80, 110, 140, true)
                text(tostring(station + 1), sx + 16, baseline + 32)
                if station == app.position then
                    sugarbee.rect(sx + 8, baseline - 36, 24, 24, 255, 215, 80, true)
                end
            end
        elseif pane.name == 'status' then
            for i, line in ipairs(status_lines) do text(line, x, y + 20 + (i - 1) * 12) end
        else
            local rows = math.max(0, math.floor((pane.h - 100) / 12))
            local first = math.max(1, #output - rows + 1)
            for i = first, #output do text(output[i], x, y + 20 + (i - first) * 12) end
        end
        if pane.name == 'console' or app.view ~= 'all' then command_strip(pane) end
        sugarbee.clip_reset()
    end
end
function shutdown() end
