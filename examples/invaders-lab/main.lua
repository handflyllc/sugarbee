-- Invaders Lab, step 1: move a ship left and right.
-- Uses only SugarBee APIs introduced in Milestones 1-3.

-- SugarBee's default logical viewport is 1280x720.
local screen_width = 1280

-- ship dimensions and coordinates
local ship_width = 60
local ship_height = 24
local ship_x = (screen_width - ship_width) / 2
local ship_y = 620
local ship_speed = 300  -- logical pixels per second (default of 300)

-- bullet coordinates, life (active), and dimensions
local bullet_active = false
local bullet_x = 0
local bullet_y = 0
local bullet_width = 4
local bullet_height = 12
local bullet_speed = 500

-- Enemy formation
local enemy_positions = {360, 480, 600, 720, 840}       -- one line of enemies
local enemy_alive = {true, true, true, true, true}      -- initialize enemy_alive to true
local enemy_start_y = 140
local enemy_y = enemy_start_y
local enemy_width = 48
local enemy_height = 24
local formation_offset = 0
local formation_descent = 24
local formation_direction = 1
local formation_speed = 100
local formation_margin = 20
local formation_left_limit = formation_margin - enemy_positions[1]
local formation_right_limit = screen_width - formation_margin - (enemy_positions[#enemy_positions] + enemy_width)

-- game state
local game_over = false
local victory = false


--[[ Required Lua callback ]]--
function init() 
    print("Invaders Lab: flight test loaded")
end

--[[ *** *** *** HELPER FUNCTIONS *** *** *** ]]--

-- FIRE BULLET
-- --------------------------------------------------------------------------
-- turn bullet on, center bullet with center of ship's x-axis, place
-- bullet at top of ship
local function fire_bullet()
    bullet_active = true
    bullet_x = ship_x + (ship_width - bullet_width) / 2
    bullet_y = ship_y - bullet_height

    print("Pew! Projectile launched")
end

-- DESCEND FORMATION
-- --------------------------------------------------------------------------
-- Move the entire enemy formation down one step
local function descend_formation()
    enemy_y = enemy_y + formation_descent

    print("Enemy formation descended to Y:", enemy_y)

    -- we enter the function with game_over set to false AND check the
    -- current, calculated bottom of enemy position against (>=) the top of
    -- the ship's defensive line. Reaching the defensive line enters the if
    -- block. It’s skipped when game_over is already true or the enemies
    -- haven’t reached the line
    if not game_over and enemy_y + enemy_height >= ship_y then
        game_over = true
        print("GAME OVER! The invaders have reached the defensive line.")
    end
end

-- RESET GAME
-- --------------------------------------------------------------------------
-- Restore game to its starting condition. Reset the ship's initial position,
-- turn off the bullet, and zero the starting position, we don't want
-- any strays! <<>> In the Enemy Formation: move the enemy formation to the
-- top position, reset the offset, reset direction to 'move right' then 
-- iterate through all enemies to revive their alive state to true.
-- <<>> Game State: reset game state variables to make the game playable
local function reset_game()
    -- Player
    ship_x = (screen_width - ship_width) / 2

    -- Projectile
    bullet_active = false
    bullet_x = 0
    bullet_y = 0

    -- Enemy formation
    enemy_y = enemy_start_y
    formation_offset = 0
    formation_direction = 1

    for i = 1, #enemy_positions do
        enemy_alive[i] = true
    end

    formation_left_limit = formation_margin - enemy_positions[1]
    formation_right_limit = screen_width - formation_margin - (enemy_positions[#enemy_positions] + enemy_width)

    -- Game state
    game_over = false   
    victory = false
    print("New Game Started!")
end

-- UPDATE(DT)
-- --------------------------------------------------------------------------
-- Because update(dt) runs at 60 Hz, we first check to see if the game_over 
-- flag is set, if it is, we exit (return) out of the function. Then we set
-- up some button recodnition for ship controls. 
function update(dt)
    -- When game is over, only allow a restart
    if game_over or victory then
        if sugarbee.btnp("b") then
            reset_game()
        end
        return
    end

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
	bullet_y = bullet_y - bullet_speed * dt
	    -- Remove the projectile when it leaves the screen.
	    if bullet_y + bullet_height < 0 then
            bullet_active = false
            print("Projectile expired")
	    end
    end
    -- Formation movement calculations
    formation_offset = formation_offset + formation_direction * formation_speed * dt
    if formation_offset >= formation_right_limit then
        formation_offset = formation_right_limit
        descend_formation()
        formation_direction = -1
    elseif formation_offset <= formation_left_limit then
        formation_offset = formation_left_limit
        descend_formation()
        formation_direction = 1
    end

    -- Reaching the defensive line ends this update before bullet collisions
    if game_over then
        return
    end
    -- Check the active bullet against living enemies
    -- ----------------------------------------------------------------------
    -- Collision: we check whether the bullet’s rectangle overlaps the
    -- enemy’s rectangle, rather than whether one coordinate falls inside it.
    if bullet_active then
        for i = 1, #enemy_positions do
            if enemy_alive[i] then
                local enemy_x = enemy_positions[i] + formation_offset

                if bullet_x < enemy_x + enemy_width
                and bullet_x + bullet_width > enemy_x
                and bullet_y < enemy_y + enemy_height
                and bullet_y + bullet_height > enemy_y then
                        enemy_alive[i] = false
                        local first_alive = nil
                        local last_alive = nil
                        for j = 1, #enemy_positions do
                            if enemy_alive[j] then
                                if first_alive == nil then
                                    first_alive = j
                                end
                                last_alive = j
                            end
                        end
                        print("Living outer slots: ", first_alive, last_alive)

                        if first_alive ~= nil then
                            formation_left_limit = formation_margin - enemy_positions[first_alive]
                            formation_right_limit = screen_width - formation_margin - (enemy_positions[last_alive] + enemy_width)
                        end

                        bullet_active = false
                        print("Enemy hit: ",i)
                        local any_alive = false
                        for j = 1, #enemy_positions do
                            if enemy_alive[j] then
                                any_alive = true
                                break
                            end
                        end
                        if not any_alive then
                            victory = true
                            print("Victory is mine!")
                        end
                        break
                end
            end
        end
    end
    -- Keep the entire ship inside the logical viewport.
    ship_x = math.max(0, math.min(screen_width - ship_width, ship_x))
end

-- DRAW()
-- --------------------------------------------------------------------------
-- This is a required Lua callback that runs per each host render iteration
-- while the game is healthy.

function draw()
    sugarbee.cls(8, 12, 32) -- RGB colors 8,12,32 (dark blue-midnight sky)
    -- Drawing: math.floor(ship_x) creates an integer drawing coordinate
    -- without changing ship_x. It doesn’t prevent floating-point calculation
    -- errors.
    -- Round down for drawing; preserve ship_x's fractional simulation position.
    local x = math.floor(ship_x)
    sugarbee.rect(x, ship_y, ship_width, ship_height,
                  80, 220, 160, true)
    sugarbee.rect(x + 24, ship_y - 12, 12, 12,
                  170, 255, 210, true)
    if bullet_active then
        sugarbee.rect(
	    math.floor(bullet_x),
	    math.floor(bullet_y),
	    bullet_width,
	    bullet_height,
	    255, 215, 100, true
        )
    end

    -- Draw the enemy formation
    for i=1, #enemy_positions do
        if enemy_alive[i] then
            local enemy_x = enemy_positions[i] + formation_offset
            
            sugarbee.rect(
            math.floor(enemy_x),
            math.floor(enemy_y),
            enemy_width, enemy_height,
            220, 100, 120, true
            )
        end
    end

    sugarbee.print("INVADERS LAB - FLIGHT TEST", 30, 25,
                   255, 255, 255, 3)
    sugarbee.print("MOVE: LEFT/RIGHT, A/D, OR GAMEPAD", 30, 60)

    if victory then
        sugarbee.print(
            "VICTORY - EARTH SAVED",
            400, 340,
            80, 255, 160, 3
        )
        sugarbee.print(
            "PRESS B TO RESTART",
            450, 390,
            255, 255, 255, 2
        )
    elseif game_over then
        sugarbee.print(
        "GAME OVER - INVADERS WIN",
        400, 340,
        255, 80, 80, 3
        )
        sugarbee.print(
        "PRESS B TO RESTART",
        450, 390,
        255, 255, 255, 2
        )
    end


end

function shutdown()
    print("Invaders Lab: Shutdown called")
end
