local editor = dofile(arg[1]).new()
local function text(s)
    local events = {}
    for c in s:gmatch('.') do events[#events+1] = {type='text', text=c} end
    return events
end
local function action(kind) return {type=kind} end
assert(editor:apply(text('ac')) == nil and editor.text == 'ac' and editor.cursor == 2)
editor:apply({action('left'), {type='text',text='b'}})
assert(editor.text == 'abc' and editor.cursor == 2)
editor:apply({action('delete'), action('home'), action('backspace')})
assert(editor.text == 'ab' and editor.cursor == 0)
editor:apply({action('right'), action('backspace'), action('end'), action('right')})
assert(editor.text == 'b' and editor.cursor == 1)
local submitted = editor:apply({action('submit'), {type='text',text='x'}})
assert(submitted == 'b' and editor.text == 'b')
for _, status in ipairs({'overflow','unsupported_text'}) do
    local s, err = editor:apply({action('submit')}, status)
    assert(s == nil and err == status and editor.text == 'b')
end
editor:apply(text(string.rep('x',255)))
assert(#editor.text == 256)
local previous = editor.text
local s, err = editor:apply({action('home'),{type='text',text='q'},action('submit')})
assert(s == nil and err == 'full' and editor.text == previous and editor.cursor == 256)
editor:apply({action('backspace')})
assert(#editor.text == 255)
s = dofile(arg[1]).new():apply({action('submit')})
assert(s == '')
print('Text line editing checks passed')
