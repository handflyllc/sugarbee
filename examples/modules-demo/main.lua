local player = require("game.player")
local hud = require("ui.hud")
local shared = require("game.shared")
assert(shared == require("game.shared"), "cache identity")
assert(shared.loads == 1, "module executed twice")

function init() player.init() end
function update(dt)
    player.update(dt)
    -- Catch a missing-module error without ending the game.
    if sugarbee.btnp("a") then
        local ok = pcall(require, "missing")
        shared.status = ok and "UNEXPECTED SUCCESS" or "Missing module caught - game continues"
    end
    if sugarbee.btnp("b") then
        -- Deliberately uncaught: verifies module filename in Guru details.
        require("game.broken")
    end
end
function draw()
    sugarbee.cls(20,25,40)
    player.draw()
    hud.draw()
end
function shutdown() end
