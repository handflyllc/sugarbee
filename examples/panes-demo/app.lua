-- Cartridge state and commands. Layout changes never recreate this state.
local textline = require('ui.textline')
local M = {}
function M.new()
    local app = {view = 'all', position = 2, moves = 0,
                 editor = textline.new(), history = {}, message = ''}
    function app:log(text)
        self.history[#self.history + 1] = text
        if #self.history > 12 then table.remove(self.history, 1) end
    end
    function app:command(line)
        self:log('> ' .. line)
        local command = line:lower():match('^%s*(.-)%s*$'):gsub('%s+', ' ')
        local view = command:match('^view (%a+)$')
        if view == 'all' or view == 'scene' or view == 'console' or view == 'status' then
            self.view = view
            self:log('View: ' .. view .. '. Use view all to restore the split.')
        elseif command == 'move left' or command == 'move right' then
            local direction = command == 'move left' and -1 or 1
            local next_position = math.max(0, math.min(4, self.position + direction))
            if next_position == self.position then self:log('The marker is at the edge.')
            else
                self.position, self.moves = next_position, self.moves + 1
                self:log('Marker moved to station ' .. (self.position + 1) .. ' of 5.')
            end
        elseif command == 'look' then
            self:log('Five stations in a quiet test room. The marker is at station ' ..
                     (self.position + 1) .. '. Move left or right to explore.')
        elseif command == 'help' then
            self:log('Commands: view all / scene / console / status; move left / right; look; help.')
        elseif command == '' then self:log('(empty command)')
        else self:log('Unknown command. Type help.') end
    end
    function app:input(events, status)
        local submitted, err = self.editor:apply(events, status)
        if err then self.message = 'Input rejected: ' .. err .. '. Line unchanged.'
        elseif #events > 0 then self.message = '' end
        if submitted ~= nil then
            self:command(submitted)
            -- Only a successful submission clears the draft, never a redraw.
            self.editor.text, self.editor.cursor = '', 0
            return true
        end
        return false
    end
    app:log('Three panes, one cartridge. Type help, then press Enter.')
    app:log('Escape pauses. All layout and command handling live in Lua.')
    return app
end
return M
