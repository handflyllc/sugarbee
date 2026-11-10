-- Silent asset fixture; the separate Music Lab exercises audible playback.
function init()
    local music = sugarbee.music
    song = assert(music.load('assets/music.json'))
    local info = assert(music.info(song))
    assert(info.track_count == 3 and info.timeline_rate == 48000)
    assert(info.end_frame == 1488000 and info.loop_eligible == true)
    assert(music.state() == "idle")
    print('Lua music load/info checks passed')
end
function update(dt) end
function draw() end
function shutdown()
    assert(sugarbee.music.unload(song))
    print('Lua music unload check passed')
end
