-- Same cartridge functions against smoke services or real Lua/native playback.
local m=sugarbee.music
for _,name in ipairs({'A','B'}) do
    local h=assert(m.load('assets/'..name..'.json'))
    local info=m.info(h)
    assert(info.track_count==1 and info.end_frame==48000 and info.timeline_rate==48000)
    assert(info.loop_eligible==true); assert(m.unload(h))
end
local function step() test_advance(); update() end
assert(song_change.current==nil and m.state()=='idle')
assert(request_song('A')); update()
if test_unavailable then
    assert(song_change.current==nil and song_change.pending==nil)
    assert(song_change.error=='audio_unavailable')
    for _=1,3 do step() end
    assert(song_change.error=='audio_unavailable')
    assert(request_song(nil)); update(); assert(song_change.error==nil)
    return
end
assert(song_change.current=='A' and m.state()=='playing'); step()
assert(request_song(nil)); assert(song_change.current==nil and m.state()=='finishing')
assert(request_song('A')); update() -- same song requested within the fade
assert(song_change.pending=='A' and song_change.current==nil)
step(); assert(song_change.current=='A' and song_change.pending==nil); step()
assert(request_song('B')); assert(request_song('A')); assert(request_song('B'))
update(); assert(song_change.pending=='B' and song_change.current==nil)
step(); assert(song_change.current=='B'); step()
assert(request_song('A')); assert(request_song(nil)); step()
assert(song_change.current==nil and song_change.pending==nil and m.state()=='idle')
-- Natural completion clears current and does not auto-restart.
assert(request_song('A')); update()
for _=1,65 do step() end
assert(song_change.current==nil and m.state()=='idle')
-- Injected race/refusal branches are policy tests, not native fault simulation.
local play=m.play
m.play=function() return false,'music_busy' end
assert(request_song('B')); update()
assert(song_change.pending=='B' and song_change.current==nil)
m.play=play; update(); assert(song_change.current=='B'); step()
local stop=m.stop
m.stop=function() return false,'session_unavailable' end
assert(not request_song('A')); assert(song_change.current=='B')
m.stop=stop; assert(request_song(nil)); step()
assert(not request_song('unknown')); assert(song_change.pending==nil)
-- Show a permanent refusal once, without retrying every update.
local attempts=0
m.play=function() attempts=attempts+1; return false,'voice_busy' end
assert(request_song('A')); update(); update()
assert(attempts==1 and song_change.pending==nil and song_change.error=='voice_busy')
m.play=play
assert(request_song('A')); update(); assert(song_change.current=='A'); step()
assert(request_song(nil)); step()
