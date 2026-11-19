local shared = require("game.shared")
local x = 100
local player = {}
function player.init() x = 100; shared.x = x end
function player.update(dt)
    x = x + 80 * dt
    if x > 1150 then x = 100 end
    shared.x = math.floor(x)
end
function player.draw()
    sugarbee.rect(math.floor(x),360,40,40,100,210,255,true)
end
return player
