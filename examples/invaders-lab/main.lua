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



function init()             -- required Lua callback, runs once when the cartridge is loaded
    print("Invaders Lab: flight test loaded") -- prints to the console
end

--[[ *** *** *** HELPER FUNCTIONS *** *** *** ]]--

-- Fire a bullet
local function fire_bullet()                            --[[ FIRE_BULLET ]]--
    bullet_active = true                                -- turn bullet on
    bullet_x = ship_x + (ship_width - bullet_width) / 2 -- center bullet with center of ship's x-axis
    bullet_y = ship_y - bullet_height                   -- place bullet at top of ship

    print("Pew! Projectile launched")
end

-- Move the entire enemy formation down one step
local function descend_formation()                      --[[ DESCEND_FORMATION ]]--
    enemy_y = enemy_y + formation_descent

    print("Enemy formation descended to Y:", enemy_y)

    if not game_over and enemy_y + enemy_height >= ship_y then              -- we enter the fuction with game_over set to false AND check the current, calculated
        game_over = true                                                    -- bottom of enemy position against (>=) the top of the ship's defensive line. If the
        print("GAME OVER! The invaders have reached the defensive line.")   -- enemy has reached the line, game_over is set to true. Either game_over = true OR
    end                                                                     -- enemy at the defensive line should bypass this if block.
end

--[[ >>> Restore the game to its starting condition <<< ]]--
local function reset_game()                             --[[ RESET_GAME ]]--
    -- Player
    ship_x = (screen_width - ship_width) / 2                                -- reset the ship's initial position

    -- Projectile
    bullet_active = false                                                   -- Turn off the bullet and zero the starting position,
    bullet_x = 0                                                            -- we don't want any strays!'
    bullet_y = 0

    -- Enemy formation
    enemy_y = enemy_start_y                                                 -- Move enemy formation to the top position
    formation_offset = 0                                                    -- reset offset
    formation_direction = 1                                                 -- reset direction to 'move right'

    for i = 1, #enemy_positions do
        enemy_alive[i] = true                                               -- iterating through all enemies to revive their alive state to true
    end

    -- Game state
    game_over = false   
    victory = false                                                    -- Game state is now playable again
    print("New Game Started!")
end

--[[ END OF HELPER FUNCTIONS ]]--

function update(dt)                                     --[[ ................. UPDATE(DT) ** ]]--
    -- When game is over, only allow a restart
    if game_over or victory then
        if sugarbee.btnp("b") then
            reset_game()
        end
        return              -- if game_over is true, we return [i.e. exit out of update(dt)]
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

    -- Check the active bullet against living enemies
    if bullet_active then                                   -- true: we have an active bullet. false: this is skipped entirely
        for i = 1, #enemy_positions do                      -- index through all available enemy_positions
            if enemy_alive[i] then                          -- true: enemy alive, continue. false: skip
                local enemy_x = enemy_positions[i] + formation_offset -- derives the enemy's current x coordinate

                if bullet_x < enemy_x + enemy_width and bullet_x + bullet_width > enemy_x and bullet_y < enemy_y + enemy_height and bullet_y + bullet_height > enemy_y then
                        enemy_alive[i] = false              -- set alive to false for the specified enemy
                        bullet_active = false               -- bullet cannot continue to be used this cycle
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
                        break                               -- we don't need to continue this check because the enemy is dead, the bullet is gone
                end
            end
        end
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
	    255, 215, 100, true   -- color, the bool represents filled or outlined
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
                   255, 255, 255, 2)
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
