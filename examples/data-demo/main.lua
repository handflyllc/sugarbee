-- Retain loaded bytes in Lua; draw() performs no file reads.
local text = assert(sugarbee.read_file('assets/message.txt'))
local bytes = assert(sugarbee.read_file('assets/bytes.bin'))
assert(bytes == string.char(0, 1, 255, 10, 0, 13))
local reads, status = 1, 'Binary check passed: 6 bytes, including zeros.'
function init() end
function update()
    if sugarbee.btnp('a') then
        local next_text, err = sugarbee.read_file('assets/message.txt')
        if next_text then
            text = next_text
            reads = reads + 1
            status = 'Text read succeeded.'
        else status = 'Read failed: ' .. err end
    end
    if sugarbee.btnp('b') then
        local absent, err = sugarbee.read_file('assets/missing.txt')
        assert(absent == nil and err == 'not_found')
        status = 'Missing file handled: ' .. err
    end
end
function draw()
    sugarbee.cls(18, 25, 42)
    sugarbee.print('CARTRIDGE DATA', 60, 50, 255, 220, 80, 3)
    sugarbee.print(text, 60, 130, 255, 255, 255, 2)
    sugarbee.print('Text reads this session: ' .. reads, 60, 280, 120, 220, 255, 2)
    sugarbee.print(status, 60, 330, 255, 220, 80, 2)
    sugarbee.print('A/J: read again   B/K: missing-file check', 60, 430, 255, 255, 255, 2)
    sugarbee.print('ENTER: Pause', 60, 480, 255, 255, 255, 2)
end
function shutdown() end
