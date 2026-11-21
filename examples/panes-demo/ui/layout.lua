-- Fixed-proportion layout for this demo's default 1280x720 canvas.
local M = {}
function M.panes(view, width, height)
    if view ~= 'all' then
        -- Scene/status retain a command strip so view all is always reachable.
        return {{name = view, x = 0, y = 0, w = width, h = height}}
    end
    local left, top = math.floor(width * 3 / 4), math.floor(height * 2 / 3)
    return {
        {name = 'scene', x = 0, y = 0, w = left, h = top},
        {name = 'console', x = 0, y = top, w = left, h = height - top},
        {name = 'status', x = left, y = 0, w = width - left, h = height}
    }
end
return M
