-- Invaders Lab, step 1: move a ship left and right.
-- Uses only SugarBee APIs introduced in Milestones 1-3.

local screen_width = 1280
local ship_width = 60
local ship_height = 24
local ship_x = (screen_width - ship_width) / 2
local ship_y = 620
local ship_speed = 300  -- logical pixels per second (default of 300)
local bullet_active = false
local bullet_x = 0
local bullet_y = 0
local bullet_width = 4
local bullet_height = 12
local bullet_speed = 500

function init()             -- required Lua callback, runs once when the cartridge is loaded
    print("Invaders Lab: flight test loaded") -- prints to the console
end

local function fire_bullet()
    bullet_active = true                                -- turn bullet on
    bullet_x = ship_x + (ship_width - bullet_width) / 2 -- center bullet with center of ship's x-axis
    bullet_y = ship_y - bullet_height                   -- place bullet at top of ship

    print("Pew! Projectile launched")
end


function update(dt)         -- required Lua callback, runs at 60 Hz
    if sugarbee.btn("left") then
        ship_x = ship_x - ship_speed * dt
    end
    if sugarbee.btn("right") then
        ship_x = ship_x + ship_speed * dt
    end
    
    -- Fire one projectile when A is newly pressed 
    if sugarbee.btnp("a") and not bullet_active then -- one bullet at a time (for now)
        fire_bullet()
    end

    -- Move the active projectile upward.
    if bullet_active then
	bullet_y = bullet_y - bullet_speed * dt -- not 100% sure what's going on here, I need a little better understanding of the coordinate system used. y=0 must be at the top!

	    -- Remove the projectile when it leaves the screen.
	    if bullet_y + bullet_height < 0 then
            bullet_active = false
            print("Projectile expired")
	    end
    end
	   
    if sugarbee.btnp("b") then
        print("B was pressed")
    end


    -- Keep the entire ship inside the logical viewport.
    ship_x = math.max(0, math.min(screen_width - ship_width, ship_x))
end

function draw()             -- required Lua callback, runs per each host render iteration while the game is healthy
    sugarbee.cls(8, 12, 32) -- clear screen, RGB colors 8,12,32 (dark blue-midnight sky)

    local x = math.floor(ship_x) -- local var 'x' to draw(), sets ship_x to integer value in the event the calculated position from above results in a float
    sugarbee.rect(x, ship_y, ship_width, ship_height,
                  80, 220, 160, true)
    sugarbee.rect(x + 24, ship_y - 12, 12, 12,
                  170, 255, 210, true)
    if bullet_active then
        sugarbee.rect(
	    math.floor(bullet_x), -- make integer
	    math.floor(bullet_y), -- make integer
	    bullet_width,         -- set at head
	    bullet_height,        -- set at head
	    255, 215, 100, true   -- color, and I forget the bool here. Will look it up.
        )
    end
    sugarbee.print("INVADERS LAB - FLIGHT TEST", 30, 25,
                   255, 255, 255, 2)
    sugarbee.print("MOVE: LEFT/RIGHT, A/D, OR GAMEPAD", 30, 60)
end
