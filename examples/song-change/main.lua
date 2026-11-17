-- Cartridge-owned policy: one current song, one replaceable pending request.
-- A/J and B/K always restart their song, even if it is already playing.
local music = sugarbee.music
local songs, load_errors = {}, {}
song_change = {current=nil, pending=nil, error=nil}

function request_song(name)
    if name ~= nil and not songs[name] then
        song_change.error = load_errors[name] or 'Unknown song'
        return false
    end
    -- A failed stop must not discard the previous request or current record.
    if music.state() ~= 'idle' then
        local ok, why = music.stop()
        if not ok then song_change.error=why; return false end
    end
    -- Clear immediately, including a same-song request during a stop fade.
    song_change.current = nil
    song_change.pending = name -- nil means cancel pending playback and stop
    song_change.error = nil
    return true
end

local function service_song_request()
    if music.state() ~= 'idle' then return end
    song_change.current = nil -- also handles natural completion
    local name = song_change.pending
    if not name then return end
    local ok, why = music.play(songs[name], {voices={1}, volume=.5, loop=false})
    if ok then
        song_change.current = name
        song_change.pending = nil
        song_change.error = nil
    else
        song_change.error = why
        -- Only a busy player is retried. Other refusals need a fresh request;
        -- do not spin forever on unavailable audio, voice contention, etc.
        if why ~= 'music_busy' then song_change.pending=nil end
    end
end

function init()
    for _, name in ipairs({'A','B'}) do
        songs[name], load_errors[name] = music.load('assets/'..name..'.json')
    end
end
function update()
    -- Stop wins if several buttons arrive together. B takes priority over A.
    if sugarbee.btnp('x') then request_song(nil)
    elseif sugarbee.btnp('b') then request_song('B')
    elseif sugarbee.btnp('a') then request_song('A') end
    service_song_request()
end
function draw()
    sugarbee.cls(24,30,40)
    sugarbee.print('SONG CHANGE',60,60,100,220,180,3)
    sugarbee.print('A / J: play or restart A (lower tone)',60,140,255,255,255,2)
    sugarbee.print('B / K: play or restart B (higher tone)',60,180,255,255,255,2)
    sugarbee.print('X / U: stop and cancel pending request',60,220,255,255,255,2)
    sugarbee.print('Current: '..(song_change.current or 'none'),60,300,255,255,255,2)
    sugarbee.print('Pending: '..(song_change.pending or 'none'),60,340,255,255,255,2)
    sugarbee.print('Transport: '..music.state(),60,380,255,255,255,2)
    sugarbee.print(song_change.error or 'Ready',60,440,255,220,120,2)
end
function shutdown()
    song_change.pending=nil
    music.stop()
    -- Native session teardown detaches playback and frees loaded songs.
end
