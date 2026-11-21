-- Game-owned single-line ASCII editor. Engine input mode and rendering stay with
-- the caller. Cursor is a zero-based insertion position; buffer limit is 256.
local M = {}
function M.new()
    local editor = {text = '', cursor = 0}
    function editor:apply(events, status)
        -- A lost/unsupported batch must never change or submit the current line.
        if status then return nil, status end
        local text, cursor = self.text, self.cursor
        for _, event in ipairs(events) do
            local kind = event.type
            if kind == 'text' then
                if #text == 256 then return nil, 'full' end
                text = text:sub(1, cursor) .. event.text .. text:sub(cursor + 1)
                cursor = cursor + 1
            elseif kind == 'backspace' and cursor > 0 then
                text = text:sub(1, cursor - 1) .. text:sub(cursor + 1)
                cursor = cursor - 1
            elseif kind == 'delete' then
                text = text:sub(1, cursor) .. text:sub(cursor + 2)
            elseif kind == 'left' then cursor = math.max(0, cursor - 1)
            elseif kind == 'right' then cursor = math.min(#text, cursor + 1)
            elseif kind == 'home' then cursor = 0
            elseif kind == 'end' then cursor = #text
            elseif kind == 'submit' then
                self.text, self.cursor = text, cursor
                return text, nil
            end
        end
        self.text, self.cursor = text, cursor
        return nil, nil
    end
    return editor
end
return M
