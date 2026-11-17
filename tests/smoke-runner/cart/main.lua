-- Exercise runner lifecycle wiring, not just the isolated service module.
local m=sugarbee.music
local data,why=sugarbee.load_save()
assert(data==nil and why=='io_error')
local song, ticks
function init()
    data,why=sugarbee.load_save(); assert(data==nil and why=='not_found')
    assert(sugarbee.save('checkpoint\0data'))
    song=assert(m.load('song.json')); ticks=0
    assert(m.play(song,{voices={1}}))
end
function update()
    ticks=ticks+1
    assert(sugarbee.load_save()=='checkpoint\0data')
    if ticks==1 then
        assert(m.state()=='playing'); assert(m.stop())
        local ok,code=m.play(song,{voices={1}})
        assert(not ok and code=='music_busy')
    elseif ticks==2 then
        assert(m.state()=='idle'); assert(m.play(song,{voices={1}}))
    end
end
function shutdown()
    assert(ticks==3); assert(sugarbee.save('shutdown'))
    assert(sugarbee.load_save()=='shutdown')
end
