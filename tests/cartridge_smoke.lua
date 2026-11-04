-- cartridge_smoke.lua: run a cartridge's Lua against a stand-in `sugarbee`
-- table, without SDL or the host binary.
--
--   lua5.4 tests/cartridge_smoke.lua <game-dir> [frames] [seed]
--
-- The stub mirrors the argument validation performed by the real bindings
-- in src/main.c and src/sb_graphics_sdl.c, so a cartridge that would hit
-- Guru Meditation on the console (non-integer coordinates, a sprite region
-- outside its texture, a stale texture handle, an RGBA buffer of the wrong
-- length, a typo'd API name) fails here instead, with a Lua traceback.
--
-- It does not render anything and cannot judge how a game looks or plays;
-- it is a crash test, not a gameplay test. Input is pseudo-random with a
-- fixed seed so failures reproduce.

local game_dir = arg[1]
local frames = tonumber(arg[2]) or 3000
local seed = tonumber(arg[3]) or 1

if _VERSION ~= "Lua 5.4" then
    io.stderr:write("cartridge_smoke: needs Lua 5.4, found " .. _VERSION .. "\n")
    os.exit(1)
end
if not game_dir then
    io.stderr:write("usage: lua5.4 tests/cartridge_smoke.lua <game-dir> [frames] [seed]\n")
    os.exit(2)
end

local DT = 1 / 60                   -- the host's fixed simulation step
local MAX_PNG_SIDE = 2048           -- docs/api/assets.md

-- ---------------------------------------------------------------------------
-- Argument checks modelled on the C bindings
-- ---------------------------------------------------------------------------
-- luaL_checkinteger accepts integers and floats with an integral value.
local function check_int(fn, pos, v)
    local i = math.tointeger(v)
    if i == nil then
        error(string.format("bad argument #%d to '%s' (integer expected, got %s)",
                            pos, fn, v == nil and "no value" or tostring(v)), 3)
    end
    return i
end

local function opt_int(fn, pos, v, default)
    if v == nil then return default end
    return check_int(fn, pos, v)
end

local function check_string(fn, pos, v)
    if type(v) ~= "string" then
        error(string.format("bad argument #%d to '%s' (string expected, got %s)",
                            pos, fn, type(v)), 3)
    end
    return v
end

-- ---------------------------------------------------------------------------
-- Stub state
-- ---------------------------------------------------------------------------
local textures = {}          -- handle -> {w = ..., h = ...}
local next_handle = 1
local held, pressed = {}, {}
local counts = {draw_calls = 0, textures = 0}

local BUTTONS = {up = true, down = true, left = true, right = true, a = true,
                 b = true, x = true, y = true, start = true, select = true}

local function button_state(fn, name, player, set)
    check_string(fn, 1, name)
    player = opt_int(fn, 2, player, 0)
    if not BUTTONS[name] or player < 0 or player > 3 then return false end
    if name == "start" then return false end     -- host-reserved
    if player ~= 0 then return false end         -- the harness drives player 0 only
    return set[name] == true
end

local function new_texture(w, h)
    local handle = next_handle
    next_handle = next_handle + 1
    textures[handle] = {w = w, h = h}
    counts.textures = counts.textures + 1
    return handle
end

local function get_texture(fn, handle)
    local t = textures[handle]
    if not t then
        error(string.format("%s: invalid texture handle %s", fn, tostring(handle)), 3)
    end
    return t
end

-- Minimal PNG header read: enough to learn the image size for bounds checks.
-- The real host decodes the whole image with libpng; here only IHDR matters.
local function png_size(path)
    local f = io.open(path, "rb")
    if not f then return nil, "cannot open" end
    local head = f:read(24)
    f:close()
    if not head or #head < 24 or head:sub(1, 8) ~= "\137PNG\r\n\26\n" or
       head:sub(13, 16) ~= "IHDR" then
        return nil, "not a PNG"
    end
    local w = string.unpack(">I4", head, 17)
    local h = string.unpack(">I4", head, 21)
    return w, h
end

-- Asset paths follow docs/api/graphics.md: relative, forward slashes, no
-- "." or ".." segments, no NUL bytes.
local function check_asset_path(path)
    if path:find("\0", 1, true) then return "asset path contains a NUL byte" end
    if path:sub(1, 1) == "/" then return "absolute paths are rejected" end
    for segment in (path .. "/"):gmatch("([^/]*)/") do
        if segment == "" or segment == "." or segment == ".." then
            return "path segment '" .. segment .. "' is rejected"
        end
    end
    return nil
end

-- ---------------------------------------------------------------------------
-- The stand-in API
-- ---------------------------------------------------------------------------
sugarbee = {}
-- Synth stubs supply execution coverage only; test_lua_synth checks the real
-- argument contract. No sound or timing is modeled by this smoke runner.
sugarbee.synth = {
    configure = function() return true end,
    note_on = function() return true end,
    note_off = function() return true end,
    stop = function() return true end
}

function sugarbee.cls(r, g, b)
    opt_int("cls", 1, r, 0); opt_int("cls", 2, g, 0); opt_int("cls", 3, b, 0)
end

function sugarbee.rect(x, y, w, h, r, g, b, filled)
    check_int("rect", 1, x); check_int("rect", 2, y)
    check_int("rect", 3, w); check_int("rect", 4, h)
    opt_int("rect", 5, r, 255); opt_int("rect", 6, g, 255); opt_int("rect", 7, b, 255)
    counts.draw_calls = counts.draw_calls + 1
end

function sugarbee.print(text, x, y, r, g, b, scale)
    check_string("print", 1, text)
    check_int("print", 2, x); check_int("print", 3, y)
    opt_int("print", 4, r, 255); opt_int("print", 5, g, 255); opt_int("print", 6, b, 255)
    opt_int("print", 7, scale, 1)
    counts.draw_calls = counts.draw_calls + 1
end

function sugarbee.texture_rgba(w, h, bytes)
    w = check_int("texture_rgba", 1, w)
    h = check_int("texture_rgba", 2, h)
    check_string("texture_rgba", 3, bytes)
    if w <= 0 or h <= 0 then error("invalid texture dimensions", 2) end
    if #bytes ~= w * h * 4 then
        error(string.format("failed to create RGBA texture (check dimensions and buffer size): " ..
                            "%dx%d needs %d bytes, got %d", w, h, w * h * 4, #bytes), 2)
    end
    return new_texture(w, h)
end

function sugarbee.texture_load(path)
    check_string("texture_load", 1, path)
    local problem = check_asset_path(path)
    if problem then error("cannot load asset '" .. path .. "': " .. problem, 2) end
    local w, h = png_size(game_dir .. "/" .. path)
    if not w then error("cannot load asset '" .. path .. "': " .. h, 2) end
    if w > MAX_PNG_SIDE or h > MAX_PNG_SIDE then
        error("invalid or oversized PNG asset '" .. path .. "'", 2)
    end
    return new_texture(w, h)
end

function sugarbee.sprite(handle, x, y)
    handle = check_int("sprite", 1, handle)
    check_int("sprite", 2, x); check_int("sprite", 3, y)
    get_texture("sprite", handle)
    counts.draw_calls = counts.draw_calls + 1
end

function sugarbee.sprite_region(handle, x, y, sx, sy, sw, sh, dw, dh, flip_x, flip_y)
    handle = check_int("sprite_region", 1, handle)
    check_int("sprite_region", 2, x); check_int("sprite_region", 3, y)
    sx = check_int("sprite_region", 4, sx); sy = check_int("sprite_region", 5, sy)
    sw = check_int("sprite_region", 6, sw); sh = check_int("sprite_region", 7, sh)
    dw = opt_int("sprite_region", 8, dw, sw)
    dh = opt_int("sprite_region", 9, dh, sh)
    local t = get_texture("sprite_region", handle)
    if sx < 0 or sy < 0 or sw <= 0 or sh <= 0 or
       sx + sw > t.w or sy + sh > t.h or dw <= 0 or dh <= 0 then
        error(string.format("invalid sprite region: src (%d,%d %dx%d) dest %dx%d in a %dx%d texture",
                            sx, sy, sw, sh, dw, dh, t.w, t.h), 2)
    end
    counts.draw_calls = counts.draw_calls + 1
end

function sugarbee.texture_free(handle)
    handle = check_int("texture_free", 1, handle)
    if textures[handle] then
        textures[handle] = nil
        return true
    end
    return false
end

function sugarbee.btn(name, player)  return button_state("btn", name, player, held) end
function sugarbee.btnp(name, player) return button_state("btnp", name, player, pressed) end

-- ---------------------------------------------------------------------------
-- Run the cartridge
-- ---------------------------------------------------------------------------
local function fail(stage, err)
    io.stderr:write(string.format("cartridge_smoke FAILED in %s (%s)\n%s\n",
                                  stage, game_dir, tostring(err)))
    os.exit(1)
end

local function call(name, stage, ...)
    local fn = _G[name]
    if type(fn) ~= "function" then return end      -- the host skips absent callbacks
    local ok, err = xpcall(fn, debug.traceback, ...)
    if not ok then fail(stage, err) end
end

local chunk, load_err = loadfile(game_dir .. "/main.lua")
if not chunk then fail("load", load_err) end
local ok, run_err = xpcall(chunk, debug.traceback)
if not ok then fail("load", run_err) end

call("init", "init()")

-- Input script: a held direction that changes now and then, occasional
-- taps on the other buttons, and a B tap every five seconds so games that
-- wait on an end screen get restarted. btnp() is true for exactly one
-- update after a press, like the host's edge buffering.
math.randomseed(seed)
local dir_names = {"up", "down", "left", "right"}
local tap_names = {"a", "b", "x", "y", "select"}
local held_dir = nil

for frame = 1, frames do
    pressed = {}
    if math.random() < 0.04 then
        local pick = math.random(#dir_names + 1)
        local new_dir = dir_names[pick]            -- nil = release everything
        if new_dir and new_dir ~= held_dir then pressed[new_dir] = true end
        held = {}
        if new_dir then held[new_dir] = true end
        held_dir = new_dir
    end
    for _, name in ipairs(tap_names) do
        if math.random() < 0.01 then pressed[name] = true; held[name] = true end
    end
    if frame % 300 == 0 then pressed.b = true; held.b = true end

    call("update", "update() at frame " .. frame, DT)
    call("draw", "draw() at frame " .. frame)

    -- taps last one frame
    for _, name in ipairs(tap_names) do held[name] = nil end
end

call("shutdown", "shutdown()")

print(string.format("cartridge_smoke OK: %s, %d frames, %d textures, %d draw calls",
                    game_dir, frames, counts.textures, counts.draw_calls))
