package.path = arg[1] .. '/?.lua;' .. package.path
local app = require('app').new()
local function input(line)
    local events = {}
    for c in line:gmatch('.') do events[#events + 1] = {type = 'text', text = c} end
    events[#events + 1] = {type = 'submit'}
    assert(app:input(events))
end
local editor = app.editor
input('  MOVE   LEFT  ')
assert(app.position == 1 and app.moves == 1)
input('move left'); input('move left')
assert(app.position == 0 and app.moves == 2)
for i = 1, 5 do input('move right') end
assert(app.position == 4 and app.moves == 6)
for _, view in ipairs({'scene', 'status', 'console', 'all'}) do
    input('view ' .. view)
    assert(app.view == view and app.position == 4 and app.moves == 6)
    assert(app.editor == editor and editor.text == '' and editor.cursor == 0)
end
input('view missing'); input('view all extra'); input('os.execute()')
assert(app.view == 'all' and app.position == 4)
assert(app.history[#app.history] == 'Unknown command. Type help.')
input(''); input('help'); input('look')
assert(app.history[#app.history]:find('station 5', 1, true))
assert(#app.history == 12)
-- Rejected batches neither submit nor mutate scene/history/draft/cursor.
app:input({{type='text',text='x'}})
local last = app.history[#app.history]
for _, status in ipairs({'overflow', 'unsupported_text'}) do
    assert(not app:input({{type='submit'}}, status))
    assert(editor.text == 'x' and editor.cursor == 1 and app.history[#app.history] == last)
end
editor.text, editor.cursor = string.rep('a',256), 256
assert(not app:input({{type='home'},{type='text',text='x'},{type='submit'}}))
assert(editor.cursor == 256 and #editor.text == 256 and app.position == 4)
assert(app:input({{type='backspace'},{type='submit'}}))
assert(editor.text == '' and app.position == 4)
-- Layout partitions the canvas exactly; maximizing doesn't touch app state.
local layout = require('ui.layout')
local split = layout.panes('all',1280,720)
assert(#split == 3)
assert(split[1].w == 960 and split[1].h == 480)
assert(split[2].y == 480 and split[2].w == 960 and split[2].h == 240)
assert(split[3].x == 960 and split[3].w == 320 and split[3].h == 720)
for _, view in ipairs({'scene','console','status'}) do
    local panes = layout.panes(view,1280,720)
    assert(#panes == 1 and panes[1].name == view and panes[1].w == 1280 and panes[1].h == 720)
end
local fresh = require('app').new()
assert(fresh.position == 2 and fresh.moves == 0 and fresh.view == 'all')
assert(fresh.editor.text == '' and #fresh.history == 2)
print('Pane commands, bounded history/input and layout checks passed')
