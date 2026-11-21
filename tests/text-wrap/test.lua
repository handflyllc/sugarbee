-- Use renderer-contract metrics; native metric correctness has separate C tests.
sugarbee = {measure_text = function(text, scale)
    assert(8 * scale <= 2147483647)
    local longest, count = 0, 0
    for line in (text .. '\n'):gmatch('(.-)\n') do
        longest = math.max(longest, #line); count = count + 1
    end
    assert(longest * 8 * scale <= 2147483647)
    assert((8 + (count - 1) * 10) * scale <= 2147483647)
    return longest * 8 * scale, (8 + (count - 1) * 10) * scale
end}
local wrap = dofile(arg[1]).wrap
local function check(text, columns, expected, height, scale)
    scale = scale or 1
    local lines, h = wrap(text, columns * 8 * scale, scale)
    assert(#lines == #expected, text)
    for i, line in ipairs(lines) do assert(line == expected[i], text .. ': line ' .. i) end
    assert(h == height * scale, text .. ': height')
end
check('x', 1, {'x'}, 8, 268435455)
check('', 4, {}, 0)
check('   ', 4, {''}, 8)
check('  a   bb  c ', 4, {'a bb', 'c'}, 18)
check('a\n\nb\n', 4, {'a', '', 'b', ''}, 38)
check('\n', 1, {'', ''}, 18)
check('abcdefgh ij', 4, {'abcd', 'efgh', 'ij'}, 28)
check('a bcdef', 4, {'a', 'bcde', 'f'}, 28)
check('ab cd', 5, {'ab cd'}, 8, 2)
check('\t\r\255', 1, {'\t', '\r', '\255'}, 28)
check(string.rep('x', 4096), 16, (function()
    local t = {}; for i=1,256 do t[i]=string.rep('x',16) end; return t
end)(), 2558)
local function refuses(...) assert(not pcall(wrap, ...)) end
refuses(3, 80); refuses('a\0b', 80); refuses(string.rep('x',4097),80)
refuses(string.rep('\n',256),80); refuses(string.rep('x',257),8)
for _, bad in ipairs({0, -1, 1.5, math.huge, 0/0, '8', false, 2147483648}) do
    refuses('a', bad); refuses('a',80,bad)
end
refuses('a',7); refuses('a',8,2)
refuses('a\nb',2147483647,268435455)
local rounded = wrap('abc', 23)
assert(#rounded == 2 and rounded[1] == 'ab' and rounded[2] == 'c')
-- Bound Lua instructions for accepted and refused worst-case layouts. This hook
-- approximates the host's instruction accounting, not native wall-clock timing.
local maximum = 0
for _, case in ipairs({{string.rep('x',4096),128},
    {string.rep('a ',2048),32768}, {string.rep('\n',255),8},
    {string.rep('x',4096),8}, {string.rep(' ',4096),8}}) do
    local count = 0
    debug.sethook(function() count = count + 100; assert(count < 1000000) end, '', 100)
    pcall(wrap, case[1], case[2])
    debug.sethook()
    assert(count < 1000000)
    maximum = math.max(maximum,count)
end
print('Text wrapping checks passed; maximum sampled Lua instructions: ' .. maximum)
