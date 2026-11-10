-- Silent cartridge fixture for music asset APIs; no playback API yet.
function init()
    local music = sugarbee.music
    song = assert(music.load('assets/music.json'))
    local info = assert(music.info(song))
    assert(info.track_count == 3 and info.timeline_rate == 48000)
    assert(info.end_frame == 1488000 and info.loop_eligible == true)
    assert(music.play == nil and music.stop == nil and music.state == nil)
    assert(music.set_volume == nil)
    print('Lua music load/info checks passed')
end
function update(dt) end
function draw() end
function shutdown()
    assert(sugarbee.music.unload(song))
    print('Lua music unload check passed')
end
