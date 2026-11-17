-- Test-only services. No JSON parsing, audio rendering, or disk save access.
local M = {}
local function arity(n, ...)
    assert(select('#', ...) == n, 'smoke API: wrong argument count')
end
local function volume(v)
    assert(type(v)=='number' and v==v and v>=0 and v<=1,
           'music: volume must be finite and in [0,1]')
end
function M.unsupported(namespace)
    return {__index=function(_, key)
        error('cartridge_smoke: unsupported API '..namespace..'.'..tostring(key), 2)
    end}
end
function M.new(config, asset_exists)
    config = config or {}
    local songs = {}
    for path, info in pairs(config.songs or {}) do
        assert(type(path)=='string' and type(info)=='table', 'invalid smoke song metadata')
        assert(math.type(info.track_count)=='integer' and info.track_count>=1 and info.track_count<=9,
               'invalid smoke track_count')
        assert(math.type(info.end_frame)=='integer' and info.end_frame>=0,
               'invalid smoke end_frame')
        assert(type(info.loop_eligible)=='boolean', 'explicit smoke loop_eligible required')
        songs[path]={track_count=info.track_count, end_frame=info.end_frame,
                     timeline_rate=48000, loop_eligible=info.loop_eligible}
    end
    assert(config.audio_available==nil or type(config.audio_available)=='boolean',
           'invalid smoke audio_available')
    local saved, ready = nil, false
    local handles, count = {}, 0
    local current, state, elapsed, looping = nil, 'idle', 0, false
    local api = {}
    function api.save(...)
        arity(1, ...)
        local data = ...
        assert(type(data)=='string', 'save: string required')
        if #data>65536 then return nil, 'too_large' end
        if not ready then return nil, 'io_error' end
        saved=data
        return true, nil
    end
    function api.load_save(...)
        arity(0, ...)
        if not ready then return nil, 'io_error' end
        if saved==nil then return nil, 'not_found' end
        return saved, nil
    end
    local music = {}
    api.music=music
    local function check(handle)
        assert(handles[handle], 'music: invalid, foreign or unloaded song handle')
        return handles[handle]
    end
    function music.load(...)
        arity(1, ...)
        local path = ...
        assert(type(path)=='string', 'music.load: string path required')
        -- The runner checks safe cartridge-relative paths before opening a file.
        if not asset_exists(path) then return nil, 'Asset not found' end
        local info=songs[path]
        assert(info, 'cartridge_smoke: music metadata required in scenario for '..path)
        if count==8 then return nil, 'music_capacity' end
        local handle=setmetatable({}, {__metatable='smoke song handle'})
        handles[handle]=info; count=count+1
        return handle
    end
    function music.info(...)
        arity(1, ...)
        local info=check(...)
        return {track_count=info.track_count, end_frame=info.end_frame,
                timeline_rate=48000, loop_eligible=info.loop_eligible}
    end
    function music.unload(...)
        arity(1, ...)
        local handle=...; check(handle)
        if current==handle then return false, 'music_in_use' end
        handles[handle]=nil; count=count-1
        return true
    end
    function music.play(...)
        arity(2, ...)
        local handle, options=...
        local info=check(handle)
        assert(type(options)=='table', 'music.play: options required')
        for key in next, options do
            assert(key=='voices' or key=='volume' or key=='loop', 'music.play: unknown option')
        end
        local voices=rawget(options,'voices')
        assert(type(voices)=='table', 'music.play: voices required')
        local used, n={}, 0
        for track, voice in next, voices do
            assert(math.type(track)=='integer' and track>=1 and track<=info.track_count,
                   'music.play: invalid track mapping')
            assert(math.type(voice)=='integer' and voice>=1 and voice<=9 and not used[voice],
                   'music.play: invalid or duplicate voice')
            used[voice]=true; n=n+1
        end
        assert(n==info.track_count, 'music.play: one voice required per declared track')
        local loop=rawget(options,'loop')
        assert(loop==nil or type(loop)=='boolean', 'music.play: boolean loop required')
        local gain=rawget(options,'volume')
        if gain~=nil then volume(gain) end
        if config.audio_available==false then return false, 'audio_unavailable' end
        if state~='idle' then return false, 'music_busy' end
        if loop and not info.loop_eligible then return false, 'loop_ineligible' end
        current, state, elapsed, looping=handle, 'playing', 0, loop==true
        return true
    end
    function music.state(...)
        arity(0, ...)
        return state
    end
    function music.stop(...)
        arity(0, ...)
        if state=='playing' then state='finishing' end
        return true
    end
    function music.set_volume(...)
        arity(1, ...); volume(...)
        if state=='idle' then return false, 'music_idle' end
        return true -- No audio or gain ramp is simulated.
    end
    setmetatable(music, M.unsupported('sugarbee.music'))
    local control={}
    function control.begin() ready=true end
    function control.advance()
        -- One synthetic finishing interval, NOT a model of envelopes/SDL timing.
        if state=='finishing' then current=nil; state='idle'
        elseif state=='playing' and not looping then
            local remaining=handles[current].end_frame-elapsed
            if remaining<=800 then state='finishing'
            else elapsed=elapsed+800 end -- one fixed 60 Hz step at 48 kHz
        end
    end
    return api, control
end
return M
