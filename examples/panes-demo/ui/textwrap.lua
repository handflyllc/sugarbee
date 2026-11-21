-- Cartridge-local layout helper for SugarBee's byte-oriented fixed-cell font.
-- Layout only: callers retain source text and own drawing, scrolling and caching.
local M = {}
local function positive_integer(value, name)
    if type(value) ~= 'number' or value ~= math.floor(value) or
       value < 1 or value > 2147483647 then
        error('textwrap: ' .. name .. ' must be a positive 32-bit integer', 3)
    end
end

function M.wrap(text, width, scale)
    if type(text) ~= 'string' then error('textwrap: text must be a string', 2) end
    if #text > 4096 then error('textwrap: input exceeds 4096 bytes', 2) end
    if text:find('\0', 1, true) then error('textwrap: embedded NUL is unsupported', 2) end
    if scale == nil then scale = 1 end
    positive_integer(width, 'width')
    positive_integer(scale, 'scale')
    local cell = sugarbee.measure_text('M', scale)
    local columns = math.floor(width / cell)
    if columns < 1 then error('textwrap: width must fit one character cell', 2) end
    local lines = {}
    local function emit(line)
        if #lines == 256 then error('textwrap: output exceeds 256 lines', 3) end
        lines[#lines + 1] = line
    end
    if text == '' then return lines, 0 end
    -- Each paragraph emits at least one line, preserving blank/trailing newlines.
    local start = 1
    while true do
        local newline = text:find('\n', start, true)
        local paragraph = text:sub(start, newline and newline - 1 or #text)
        local line = ''
        for word in paragraph:gmatch('[^ ]+') do
            if line ~= '' then
                if #line + 1 + #word <= columns then
                    line = line .. ' ' .. word
                    word = ''
                else
                    emit(line)
                    line = ''
                end
            end
            local offset = 1
            while #word - offset + 1 > columns do
                emit(word:sub(offset, offset + columns - 1))
                offset = offset + columns
            end
            if word ~= '' then line = word:sub(offset) end
        end
        emit(line)
        if not newline then break end
        start = newline + 1
    end
    -- Measuring joined lines uses the renderer's exact final-line height rules.
    -- A single blank line still has a cell height, unlike empty source text.
    local joined = table.concat(lines, '\n')
    local _, height = sugarbee.measure_text(joined == '' and ' ' or joined, scale)
    return lines, height
end
return M
