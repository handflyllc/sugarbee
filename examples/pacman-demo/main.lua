-- ============================================================================
-- Pac-Man style demonstrator for SugarBee
-- ============================================================================
-- Single level, three lives, four ghosts with classic-style targeting,
-- power pellets and a side tunnel. Uses only documented SugarBee v0.1 APIs:
-- cls / rect / print, texture_rgba + sprite_region, btn / btnp.
--
-- Everything on screen is either a rect or a sprite generated in init()
-- with texture_rgba(), so the cartridge needs no PNG assets.
--
-- Controls: D-pad / arrows / WASD to steer, B (keyboard K) to restart.
-- (Start is host-reserved; A/J is also used by the launcher.)

-- ---------------------------------------------------------------------------
-- Tunables
-- ---------------------------------------------------------------------------
local TILE          = 22        -- logical pixels per maze tile
local MAZE_X        = 40        -- top-left of the maze on the 1280x720 screen
local MAZE_Y        = 19
local SPRITE        = 20        -- sprite cell size in pixels

local PLAYER_SPEED  = 150       -- logical pixels per second
local GHOST_SPEED   = 140
local FRIGHT_SPEED  = 75        -- ghosts while frightened
local EYES_SPEED    = 280       -- eyes returning to the pen
local TUNNEL_SPEED  = 70        -- ghosts inside the side tunnel

local FRIGHT_TIME   = 6.0       -- seconds of frightened mode
local FLASH_TIME    = 2.0       -- last seconds of fright flash white
local READY_TIME    = 2.0
local DEATH_TIME    = 1.5
local EAT_PAUSE     = 0.5       -- freeze while showing a ghost's score

local DOT_SCORE     = 10
local POWER_SCORE   = 50
local GHOST_SCORES  = {200, 400, 800, 1600}

-- Scatter/chase schedule in seconds; the final chase lasts forever.
local MODE_SCHEDULE = {
    {"scatter", 7}, {"chase", 20}, {"scatter", 7}, {"chase", 20},
    {"scatter", 5}, {"chase", math.huge},
}

-- ---------------------------------------------------------------------------
-- Maze
-- ---------------------------------------------------------------------------
-- '#' wall     '.' dot     'o' power pellet     ' ' open floor (no dot)
-- 'x' void outside the maze (not walkable, not drawn)
-- '-' ghost-pen door: ghosts pass when leaving or returning, never the player
--
-- Row 14 is the tunnel: its two ends wrap around to each other.
local MAZE_ROWS = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.#####.##.#####.######",
    "xxxxx#.#####.##.#####.#xxxxx",
    "xxxxx#.##..........##.#xxxxx",
    "xxxxx#.##.###--###.##.#xxxxx",
    "######.##.#      #.##.######",
    "      .   #      #   .      ",
    "######.##.########.##.######",
    "xxxxx#.##..........##.#xxxxx",
    "xxxxx#.##.########.##.#xxxxx",
    "xxxxx#.##.########.##.#xxxxx",
    "######.##.########.##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#.####.#####.##.#####.####.#",
    "#o..##.......  .......##..o#",
    "###.##.##.########.##.##.###",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#.##########.##.##########.#",
    "#..........................#",
    "############################",
}

local MAP_W = #MAZE_ROWS[1]
local MAP_H = #MAZE_ROWS
local MAZE_PX_W = MAP_W * TILE

local PLAYER_START = {col = 14, row = 23}
local PEN_EXIT     = {col = 14, row = 11}   -- tile just above the door
local PEN_CENTER   = {col = 14, row = 13}   -- where returning eyes park

-- map[row][col] holds one character per tile; dots are removed as eaten.
local map = {}
local dots_left = 0

-- Directions. Order matters: ghosts break distance ties in this order.
local DIRS = {
    up    = {dx = 0,  dy = -1},
    left  = {dx = -1, dy = 0},
    down  = {dx = 0,  dy = 1},
    right = {dx = 1,  dy = 0},
}
local DIR_ORDER = {"up", "left", "down", "right"}
local OPPOSITE  = {up = "down", down = "up", left = "right", right = "left"}

-- ---------------------------------------------------------------------------
-- Game state
-- ---------------------------------------------------------------------------
local score, lives = 0, 3
local state = "ready"        -- ready | play | eat_pause | dying | won | lost
local state_timer = 0
local mode_index, mode_timer = 1, 0
local fright_timer = 0
local ghosts_eaten = 0        -- during the current fright, for score escalation
local eat_popup = nil         -- {x, y, value} shown during eat_pause
local anim_clock = 0          -- drives every animation
local level_timer = 0         -- time since the maze started, for pen releases

local player = {}
local ghosts = {}

-- texture handles, filled in init()
local tex = {}

-- ---------------------------------------------------------------------------
-- Maze helpers
-- ---------------------------------------------------------------------------
local function wrap_col(col)
    return ((col - 1) % MAP_W) + 1
end

local function tile_at(col, row)
    if row < 1 or row > MAP_H then return "x" end
    return map[row][wrap_col(col)]
end

-- Can an entity enter this tile? Ghosts in "exit"/"eyes" mode may use the
-- pen door; nobody else may.
local function walkable(col, row, through_door)
    local t = tile_at(col, row)
    if t == "." or t == "o" or t == " " then return true end
    if t == "-" then return through_door == true end
    return false
end

-- Pixel position (relative to the maze origin) of a tile's center.
local function tile_center(col, row)
    return (col - 0.5) * TILE, (row - 0.5) * TILE
end

-- Which tile is this pixel position in? May return col 0 or MAP_W + 1
-- inside the tunnel; tile_at() wraps those.
local function tile_of(x, y)
    return math.floor(x / TILE) + 1, math.floor(y / TILE) + 1
end

local function tile_dist2(c1, r1, c2, r2)
    local dc, dr = c1 - c2, r1 - r2
    return dc * dc + dr * dr
end

local function load_map()
    dots_left = 0
    for r = 1, MAP_H do
        map[r] = {}
        local line = MAZE_ROWS[r]
        assert(#line == MAP_W, "maze row " .. r .. " has wrong length")
        for c = 1, MAP_W do
            local ch = line:sub(c, c)
            map[r][c] = ch
            if ch == "." or ch == "o" then dots_left = dots_left + 1 end
        end
    end
end

-- ---------------------------------------------------------------------------
-- Movement along the tile grid
-- ---------------------------------------------------------------------------
-- Entities move continuously, but decisions (turns, stops) happen only at
-- tile centers. move_entity() advances up to `dist` pixels; whenever the
-- path crosses a tile center it snaps there and calls `at_center(e)`,
-- which may change e.dir or set it to nil (stopped).
local function move_entity(e, dist, at_center)
    while dist > 0 and e.dir do
        local d = DIRS[e.dir]
        local col, row = tile_of(e.x, e.y)
        local cx, cy = tile_center(col, row)
        -- signed distance to this tile's center along the travel direction
        local ahead = (cx - e.x) * d.dx + (cy - e.y) * d.dy
        if ahead <= 0 then
            -- center is behind us (or we are on it): next center is one tile on
            ahead = ahead + TILE
            cx, cy = cx + d.dx * TILE, cy + d.dy * TILE
        end
        if ahead > dist then
            e.x = e.x + d.dx * dist
            e.y = e.y + d.dy * dist
            dist = 0
        else
            e.x, e.y = cx, cy
            dist = dist - ahead
            at_center(e)
        end
    end
    -- tunnel wrap: keep the center x inside [-TILE/2, MAZE_PX_W + TILE/2)
    if e.x < -TILE / 2 then
        e.x = e.x + MAZE_PX_W
    elseif e.x >= MAZE_PX_W + TILE / 2 then
        e.x = e.x - MAZE_PX_W
    end
end

local function in_tunnel(e)
    local col, row = tile_of(e.x, e.y)
    return row == 14 and (col <= 6 or col >= 23)
end

-- ---------------------------------------------------------------------------
-- Player
-- ---------------------------------------------------------------------------
-- player.dir is the direction of travel, or nil while stopped at a wall.
-- player.facing is the last direction of travel and is only used to pick
-- the sprite, so a stopped Pac-Man keeps looking at the wall it hit.
local function reset_player()
    player.x, player.y = tile_center(PLAYER_START.col, PLAYER_START.row)
    player.dir = "left"
    player.facing = "left"
    player.wanted = "left"
end

local function player_at_center(e)
    local col, row = tile_of(e.x, e.y)
    local w = DIRS[e.wanted]
    if walkable(col + w.dx, row + w.dy, false) then
        e.dir = e.wanted
    else
        local d = DIRS[e.dir]
        if not walkable(col + d.dx, row + d.dy, false) then
            e.dir = nil        -- bumped into a wall; wait at the center
        end
    end
end

local function read_player_input()
    local wanted = nil
    if sugarbee.btn("up") then wanted = "up"
    elseif sugarbee.btn("down") then wanted = "down"
    elseif sugarbee.btn("left") then wanted = "left"
    elseif sugarbee.btn("right") then wanted = "right"
    end
    if not wanted then return end
    player.wanted = wanted
    -- A reversal is always legal and takes effect immediately; it is the
    -- one turn that never has to wait for a tile center.
    if player.dir and wanted == OPPOSITE[player.dir] then
        player.dir = wanted
    elseif not player.dir then
        -- stopped against a wall: start again if the new direction is open
        local col, row = tile_of(player.x, player.y)
        local d = DIRS[wanted]
        if walkable(col + d.dx, row + d.dy, false) then
            player.dir = wanted
        end
    end
end

-- forward declarations (defined further down, used by update_player)
local start_fright, enter_dying, win_level

local function update_player(dt)
    read_player_input()
    move_entity(player, PLAYER_SPEED * dt, player_at_center)
    if player.dir then player.facing = player.dir end

    -- eat whatever is under the player's center
    local col, row = tile_of(player.x, player.y)
    col = wrap_col(col)
    local t = map[row] and map[row][col]
    if t == "." then
        map[row][col] = " "
        score = score + DOT_SCORE
        dots_left = dots_left - 1
    elseif t == "o" then
        map[row][col] = " "
        score = score + POWER_SCORE
        dots_left = dots_left - 1
        start_fright()
    end
    if dots_left <= 0 then win_level() end
end

-- ---------------------------------------------------------------------------
-- Ghosts
-- ---------------------------------------------------------------------------
-- Ghost states:
--   pen    : bobbing inside the pen until release_at seconds have elapsed
--   exit   : moving to the door column, then straight up through the door
--   normal : scatter/chase/frightened movement on the grid
--   eyes   : eaten; racing back to the tile above the door
--   enter  : dropping through the door into the pen, then back to exit
local GHOST_DEFS = {
    {name = "blinky", color = {255, 0,   0  }, scatter = {col = 26, row = 1 },
     start = {col = 14, row = 11}, release_at = 0, outside = true},
    {name = "pinky",  color = {255, 184, 255}, scatter = {col = 3,  row = 1 },
     start = {col = 14, row = 13}, release_at = 0},
    {name = "inky",   color = {0,   255, 255}, scatter = {col = 26, row = 30},
     start = {col = 12, row = 13}, release_at = 4},
    {name = "clyde",  color = {255, 184, 82 }, scatter = {col = 3,  row = 30},
     start = {col = 16, row = 13}, release_at = 8},
}

local function reset_ghosts()
    ghosts = {}
    for i, def in ipairs(GHOST_DEFS) do
        local g = {def = def, index = i}
        g.x, g.y = tile_center(def.start.col, def.start.row)
        if def.outside then
            g.state = "normal"
            g.dir = "left"
        else
            g.state = "pen"
            g.dir = "up"
            g.bob = 0
        end
        g.frightened = false
        ghosts[i] = g
    end
end

local function current_mode()
    return MODE_SCHEDULE[mode_index][1]
end

-- Where is this ghost trying to go right now? Returns a tile (col, row).
local function ghost_target(g)
    if g.state == "eyes" then
        return PEN_EXIT.col, PEN_EXIT.row
    end
    local pcol, prow = tile_of(player.x, player.y)
    pcol = wrap_col(pcol)
    if current_mode() == "scatter" then
        return g.def.scatter.col, g.def.scatter.row
    end
    local pd = DIRS[player.facing]
    local name = g.def.name
    if name == "blinky" then
        return pcol, prow
    elseif name == "pinky" then
        return pcol + pd.dx * 4, prow + pd.dy * 4
    elseif name == "inky" then
        -- two tiles ahead of the player, then double the vector from Blinky
        local ax, ay = pcol + pd.dx * 2, prow + pd.dy * 2
        local b = ghosts[1]
        local bcol, brow = tile_of(b.x, b.y)
        return ax + (ax - bcol), ay + (ay - brow)
    else -- clyde: chase when far away, retreat to the corner when close
        local gcol, grow = tile_of(g.x, g.y)
        if tile_dist2(gcol, grow, pcol, prow) > 64 then
            return pcol, prow
        end
        return g.def.scatter.col, g.def.scatter.row
    end
end

local function ghost_at_center(g)
    local col, row = tile_of(g.x, g.y)
    local through_door = (g.state == "eyes")

    -- Eyes that have reached the tile above the door drop into the pen.
    if g.state == "eyes" and wrap_col(col) == PEN_EXIT.col and row == PEN_EXIT.row then
        g.state = "enter"
        g.dir = "down"
        return
    end

    local options = {}
    for _, name in ipairs(DIR_ORDER) do
        if name ~= OPPOSITE[g.dir] then
            local d = DIRS[name]
            if walkable(col + d.dx, row + d.dy, through_door) then
                options[#options + 1] = name
            end
        end
    end
    if #options == 0 then
        g.dir = OPPOSITE[g.dir]        -- dead end: the only way is back
        return
    end
    if g.frightened then
        g.dir = options[math.random(#options)]
        return
    end
    local tcol, trow = ghost_target(g)
    local best, best_d = nil, math.huge
    for _, name in ipairs(options) do
        local d = DIRS[name]
        local dist = tile_dist2(col + d.dx, row + d.dy, tcol, trow)
        if dist < best_d then best, best_d = name, dist end
    end
    g.dir = best
end

-- Straight-line movement used inside the pen, where the grid rules do
-- not apply. Returns true when the target has been reached.
local function move_toward(g, tx, ty, dist)
    local dx, dy = tx - g.x, ty - g.y
    local len = math.sqrt(dx * dx + dy * dy)
    if len <= dist then
        g.x, g.y = tx, ty
        return true
    end
    g.x = g.x + dx / len * dist
    g.y = g.y + dy / len * dist
    return false
end

local function ghost_speed(g)
    if g.state == "eyes" then return EYES_SPEED end
    if g.frightened then return FRIGHT_SPEED end
    if in_tunnel(g) then return TUNNEL_SPEED end
    return GHOST_SPEED
end

local function update_ghost(g, dt)
    if g.state == "pen" then
        -- bob up and down until it is time to leave
        g.bob = g.bob + dt
        g.dir = (math.floor(g.bob * 2) % 2 == 0) and "up" or "down"
        if level_timer >= g.def.release_at then
            g.state = "exit"
        end
    elseif g.state == "exit" then
        -- slide sideways to the door column, then straight up and out
        local exit_x, exit_y = tile_center(PEN_EXIT.col, PEN_EXIT.row)
        if math.abs(g.x - exit_x) > 0.5 then
            g.dir = (exit_x > g.x) and "right" or "left"
            move_toward(g, exit_x, g.y, GHOST_SPEED * 0.5 * dt)
        else
            g.dir = "up"
            if move_toward(g, exit_x, exit_y, GHOST_SPEED * 0.5 * dt) then
                g.state = "normal"
                g.dir = "left"
            end
        end
    elseif g.state == "enter" then
        g.dir = "down"
        local px, py = tile_center(PEN_CENTER.col, PEN_CENTER.row)
        if move_toward(g, px, py, EYES_SPEED * dt) then
            g.state = "exit"
            g.frightened = false
        end
    else
        move_entity(g, ghost_speed(g) * dt, ghost_at_center)
    end
end

-- Every ghost on the grid turns around. Used on scatter/chase switches and
-- when a power pellet is eaten.
local function reverse_ghosts()
    for _, g in ipairs(ghosts) do
        if g.state == "normal" then
            g.dir = OPPOSITE[g.dir]
        end
    end
end

local function update_modes(dt)
    if fright_timer > 0 then
        fright_timer = fright_timer - dt
        if fright_timer <= 0 then
            fright_timer = 0
            for _, g in ipairs(ghosts) do g.frightened = false end
        end
        return   -- the scatter/chase clock pauses while ghosts are frightened
    end
    mode_timer = mode_timer + dt
    if mode_timer >= MODE_SCHEDULE[mode_index][2] then
        mode_timer = 0
        mode_index = math.min(mode_index + 1, #MODE_SCHEDULE)
        reverse_ghosts()
    end
end

function start_fright()
    fright_timer = FRIGHT_TIME
    ghosts_eaten = 0
    for _, g in ipairs(ghosts) do
        if g.state ~= "eyes" and g.state ~= "enter" then
            g.frightened = true
        end
    end
    reverse_ghosts()
end

-- ---------------------------------------------------------------------------
-- Collisions and state transitions
-- ---------------------------------------------------------------------------
function enter_dying()
    state = "dying"
    state_timer = DEATH_TIME
end

function win_level()
    state = "won"
end

local function check_collisions()
    for _, g in ipairs(ghosts) do
        if g.state == "normal" then
            local dx, dy = g.x - player.x, g.y - player.y
            if dx * dx + dy * dy < (TILE * 0.6) ^ 2 then
                if g.frightened then
                    ghosts_eaten = ghosts_eaten + 1
                    local value = GHOST_SCORES[math.min(ghosts_eaten, #GHOST_SCORES)]
                    score = score + value
                    g.state = "eyes"
                    g.frightened = false
                    eat_popup = {x = g.x, y = g.y, value = value}
                    state = "eat_pause"
                    state_timer = EAT_PAUSE
                    return
                else
                    enter_dying()
                    return
                end
            end
        end
    end
end

local function start_round()
    reset_player()
    reset_ghosts()
    mode_index, mode_timer = 1, 0
    fright_timer = 0
    level_timer = 0
    eat_popup = nil
    state = "ready"
    state_timer = READY_TIME
end

local function new_game()
    score, lives = 0, 3
    load_map()
    start_round()
end

-- ---------------------------------------------------------------------------
-- Sprite generation
-- ---------------------------------------------------------------------------
-- build_texture() calls shade(x, y) for every pixel of a w x h image and
-- expects r, g, b, a back (or nil for transparent). The bytes are packed
-- into one Lua string in R, G, B, A order and uploaded once.
local function build_texture(w, h, shade)
    local bytes = {}
    for y = 0, h - 1 do
        for x = 0, w - 1 do
            local r, g, b, a = shade(x, y)
            if r == nil then
                bytes[#bytes + 1] = "\0\0\0\0"
            else
                bytes[#bytes + 1] = string.char(r, g, b, a or 255)
            end
        end
    end
    return sugarbee.texture_rgba(w, h, table.concat(bytes))
end

-- Pac-Man sheet: 4 directions x 3 mouth frames, each SPRITE x SPRITE.
-- Column = direction index (up, left, down, right), row = mouth frame.
local MOUTH_HALF_ANGLE = {0, 0.45, 0.95}       -- radians, per frame
local DIR_ANGLE = {up = -math.pi / 2, left = math.pi, down = math.pi / 2, right = 0}
local DIR_INDEX = {up = 0, left = 1, down = 2, right = 3}

local function make_pacman_sheet()
    local half = SPRITE / 2
    local radius = half - 0.5
    return build_texture(SPRITE * 4, SPRITE * 3, function(px, py)
        local frame = math.floor(py / SPRITE)
        local dir = math.floor(px / SPRITE)
        local x, y = px % SPRITE - half + 0.5, py % SPRITE - half + 0.5
        if x * x + y * y > radius * radius then return nil end
        local mouth = MOUTH_HALF_ANGLE[frame + 1]
        if mouth > 0 then
            local facing = DIR_ANGLE[DIR_ORDER[dir + 1]]
            local a = math.atan(y, x) - facing
            a = (a + math.pi) % (2 * math.pi) - math.pi   -- normalize to [-pi, pi]
            if math.abs(a) < mouth then return nil end
        end
        return 255, 230, 0, 255
    end)
end

-- Ghost body: dome on top, skirt with bumps at the bottom. Two frames side
-- by side so the skirt can wiggle. Eyes are a separate overlay sprite.
local function make_ghost_body(r, g, b)
    local half = SPRITE / 2
    return build_texture(SPRITE * 2, SPRITE, function(px, py)
        local frame = math.floor(px / SPRITE)
        local x = px % SPRITE
        local cx, cy = x - half + 0.5, py - half + 0.5
        -- dome
        if py < half then
            if cx * cx + cy * cy > (half - 0.5) ^ 2 then return nil end
        else
            if x < 1 or x >= SPRITE - 1 then return nil end
            -- skirt: three bumps; the two frames are offset by half a bump
            local bump_w = 6
            local phase = (x + frame * 3) % bump_w
            local depth = (phase < 3) and 0 or 2
            if py >= SPRITE - depth then return nil end
        end
        return r, g, b, 255
    end)
end

-- Eye overlay sheet: 4 directions, whites with pupils shifted toward the
-- look direction. Drawn on top of the body (and alone for returning eyes).
local function make_eye_sheet()
    return build_texture(SPRITE * 4, SPRITE, function(px, py)
        local d = DIRS[DIR_ORDER[math.floor(px / SPRITE) + 1]]
        local x = px % SPRITE
        for _, ex in ipairs({6, 13}) do          -- left eye, right eye
            local ey = 7
            -- pupil: a 2x2 block pushed one pixel toward the look direction
            local pcx, pcy = ex + d.dx * 1.5, ey + d.dy * 1.5
            if math.abs(x - pcx) <= 1 and math.abs(py - pcy) <= 1 then
                return 20, 20, 120, 255
            end
            -- white: a small ellipse, 4 wide by 5 tall
            local wx, wy = x - ex, py - ey
            if wx * wx / 4 + wy * wy / 6.25 <= 1 then
                return 255, 255, 255, 255
            end
        end
        return nil
    end)
end

-- Frightened face overlay: two dots for eyes and a zigzag mouth.
local function make_fright_face(r, g, b)
    return build_texture(SPRITE, SPRITE, function(x, y)
        if (x == 6 or x == 7 or x == 12 or x == 13) and (y == 7 or y == 8) then
            return r, g, b, 255
        end
        if y == 13 and (x == 4 or x == 7 or x == 10 or x == 13) then return r, g, b, 255 end
        if y == 14 and x >= 4 and x <= 15 and x ~= 4 and x ~= 7 and x ~= 10 and x ~= 13 then
            return r, g, b, 255
        end
        return nil
    end)
end

-- ---------------------------------------------------------------------------
-- SugarBee callbacks
-- ---------------------------------------------------------------------------
function init()
    tex.pacman = make_pacman_sheet()
    tex.eyes = make_eye_sheet()
    tex.body = {}
    for i, def in ipairs(GHOST_DEFS) do
        tex.body[i] = make_ghost_body(def.color[1], def.color[2], def.color[3])
    end
    tex.fright_body = make_ghost_body(33, 33, 255)
    tex.flash_body  = make_ghost_body(255, 255, 255)
    tex.fright_face = make_fright_face(255, 200, 200)
    tex.flash_face  = make_fright_face(255, 0, 0)
    new_game()
    print("Pac-Man demo: " .. dots_left .. " dots loaded")
end

function update(dt)
    anim_clock = anim_clock + dt

    if state == "won" or state == "lost" then
        if sugarbee.btnp("b") then new_game() end
        return
    end

    if state == "ready" then
        state_timer = state_timer - dt
        if state_timer <= 0 then state = "play" end
        return
    end

    if state == "eat_pause" then
        state_timer = state_timer - dt
        if state_timer <= 0 then
            eat_popup = nil
            state = "play"
        end
        return
    end

    if state == "dying" then
        state_timer = state_timer - dt
        if state_timer <= 0 then
            lives = lives - 1
            if lives <= 0 then
                state = "lost"
            else
                start_round()
            end
        end
        return
    end

    -- state == "play"
    level_timer = level_timer + dt
    update_modes(dt)
    update_player(dt)
    if state ~= "play" then return end      -- the last dot ended the level
    for _, g in ipairs(ghosts) do update_ghost(g, dt) end
    check_collisions()
end

-- ---------------------------------------------------------------------------
-- Drawing
-- ---------------------------------------------------------------------------
-- Simulation positions are floats; everything is floored here, at the
-- rendering boundary, because the rect/sprite bindings take integers.
local function screen_xy(x, y)
    return math.floor(MAZE_X + x), math.floor(MAZE_Y + y)
end

local function draw_maze()
    local blink_on = math.floor(anim_clock * 4) % 2 == 0
    for r = 1, MAP_H do
        for c = 1, MAP_W do
            local t = map[r][c]
            local sx, sy = MAZE_X + (c - 1) * TILE, MAZE_Y + (r - 1) * TILE
            if t == "#" then
                sugarbee.rect(sx, sy, TILE, TILE, 24, 24, 140, true)
                sugarbee.rect(sx + 3, sy + 3, TILE - 6, TILE - 6, 40, 40, 220, true)
            elseif t == "." then
                sugarbee.rect(sx + TILE // 2 - 2, sy + TILE // 2 - 2, 4, 4, 255, 184, 150, true)
            elseif t == "o" and blink_on then
                sugarbee.rect(sx + TILE // 2 - 5, sy + TILE // 2 - 5, 10, 10, 255, 184, 150, true)
            elseif t == "-" then
                sugarbee.rect(sx, sy + TILE // 2 - 2, TILE, 4, 255, 184, 255, true)
            end
        end
    end
end

local function draw_player()
    local frame = 0
    if player.dir then
        local seq = {0, 1, 2, 1}
        frame = seq[math.floor(anim_clock * 16) % 4 + 1]
    end
    local col = DIR_INDEX[player.facing]
    local sx, sy = screen_xy(player.x - SPRITE / 2, player.y - SPRITE / 2)
    if state == "dying" then
        -- death effect: the wide-open frame shrinks away over DEATH_TIME
        frame = 2
        local size = math.max(2, math.floor(SPRITE * state_timer / DEATH_TIME))
        local off = (SPRITE - size) // 2
        sugarbee.sprite_region(tex.pacman, sx + off, sy + off,
                               col * SPRITE, frame * SPRITE, SPRITE, SPRITE,
                               size, size, false, false)
    else
        sugarbee.sprite_region(tex.pacman, sx, sy,
                               col * SPRITE, frame * SPRITE, SPRITE, SPRITE)
    end
end

local function draw_ghost(g)
    local sx, sy = screen_xy(g.x - SPRITE / 2, g.y - SPRITE / 2)
    local frame = math.floor(anim_clock * 6) % 2
    local look = DIR_INDEX[g.dir or "left"]
    if g.state == "eyes" or g.state == "enter" then
        sugarbee.sprite_region(tex.eyes, sx, sy, look * SPRITE, 0, SPRITE, SPRITE)
        return
    end
    if g.frightened then
        local flashing = fright_timer < FLASH_TIME and math.floor(anim_clock * 8) % 2 == 0
        local body = flashing and tex.flash_body or tex.fright_body
        local face = flashing and tex.flash_face or tex.fright_face
        sugarbee.sprite_region(body, sx, sy, frame * SPRITE, 0, SPRITE, SPRITE)
        sugarbee.sprite(face, sx, sy)
    else
        sugarbee.sprite_region(tex.body[g.index], sx, sy, frame * SPRITE, 0, SPRITE, SPRITE)
        sugarbee.sprite_region(tex.eyes, sx, sy, look * SPRITE, 0, SPRITE, SPRITE)
    end
end

local function draw_hud()
    local hx = MAZE_X + MAZE_PX_W + 60
    sugarbee.print("SUGARBEE", hx, 40, 255, 230, 0, 4)
    sugarbee.print("PAC-DEMO", hx, 80, 255, 230, 0, 4)

    sugarbee.print("SCORE", hx, 160, 255, 255, 255, 2)
    sugarbee.print(tostring(score), hx, 185, 255, 255, 255, 3)

    sugarbee.print("DOTS LEFT", hx, 250, 255, 255, 255, 2)
    sugarbee.print(tostring(dots_left), hx, 275, 255, 184, 150, 3)

    sugarbee.print("LIVES", hx, 340, 255, 255, 255, 2)
    for i = 1, lives - 1 do
        sugarbee.sprite_region(tex.pacman, hx + (i - 1) * (SPRITE + 6), 365,
                               DIR_INDEX.right * SPRITE, SPRITE, SPRITE, SPRITE)
    end

    local mode = (fright_timer > 0) and "FRIGHTENED" or current_mode():upper()
    sugarbee.print("GHOSTS: " .. mode, hx, 430, 160, 160, 160, 2)

    sugarbee.print("D-PAD / WASD: MOVE", hx, 560, 120, 120, 120)
    sugarbee.print("B (K): RESTART", hx, 575, 120, 120, 120)
    sugarbee.print("ENTER: PAUSE MENU", hx, 590, 120, 120, 120)
end

local function draw_center_text(text, y, r, g, b, scale)
    local w = #text * 8 * scale
    sugarbee.print(text, math.floor(MAZE_X + (MAZE_PX_W - w) / 2), y, r, g, b, scale)
end

function draw()
    sugarbee.cls(0, 0, 0)
    draw_maze()

    draw_player()
    for _, g in ipairs(ghosts) do draw_ghost(g) end

    -- cover the tunnel mouths so wrapping sprites vanish at the maze edge
    local ty = MAZE_Y + 13 * TILE
    sugarbee.rect(0, ty - 2, MAZE_X, TILE + 4, 0, 0, 0, true)
    sugarbee.rect(MAZE_X + MAZE_PX_W, ty - 2, 40, TILE + 4, 0, 0, 0, true)

    if eat_popup then
        local sx, sy = screen_xy(eat_popup.x - 12, eat_popup.y - 4)
        sugarbee.print(tostring(eat_popup.value), sx, sy, 80, 255, 255)
    end

    draw_hud()

    local msg_y = MAZE_Y + 16 * TILE + 3   -- the open row below the pen
    if state == "ready" then
        draw_center_text("READY!", msg_y, 255, 230, 0, 2)
    elseif state == "won" then
        draw_center_text("LEVEL CLEARED!", msg_y, 80, 255, 160, 2)
        draw_center_text("PRESS B TO PLAY AGAIN", msg_y + 24, 255, 255, 255, 1)
    elseif state == "lost" then
        draw_center_text("GAME OVER", msg_y, 255, 80, 80, 2)
        draw_center_text("PRESS B TO PLAY AGAIN", msg_y + 24, 255, 255, 255, 1)
    end
end

function shutdown()
    print("Pac-Man demo: shutdown, final score " .. score)
end
