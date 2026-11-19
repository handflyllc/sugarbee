local shared = require("game.shared")
return {
    draw = function()
        sugarbee.print("MODULES DEMO",40,40,255,255,255,3)
        sugarbee.print("Shared module loads: "..shared.loads.." (must be 1)",40,110,255,255,255,2)
        sugarbee.print("Player X: "..shared.x,40,155,255,255,255,2)
        sugarbee.print(shared.status,40,210,255,210,80,2)
        sugarbee.print("A: caught missing module   B: intentional Guru",40,550,255,255,255,2)
        sugarbee.print("START pauses. Relaunch resets position and load count.",40,610,255,255,255,2)
    end
}
