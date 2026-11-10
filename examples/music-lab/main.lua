-- One backing track uses voices 1..3; an independent effect uses voice 9.
local music, synth = sugarbee.music, sugarbee.synth
local song
local looping, effect = false, false
local volumes, level = {.8, .3, 0}, 1
local status = "Ready"
local function report(ok, reason)
    status = ok and "OK" or reason
    return ok
end
function init()
    song = assert(music.load('assets/backing.json'))
    assert(music.info(song).loop_eligible)
    report(synth.configure(9, {waveform='pulse', pulse_width=.25,
        attack=.02, decay=0, sustain=1, release=.1, volume=.2, pan=.7}))
end
function update(dt)
    if sugarbee.btnp('up') or sugarbee.btnp('down') then looping=not looping end
    if sugarbee.btnp('x') then
        level=level%#volumes+1
        if music.state()~='idle' then report(music.set_volume(volumes[level])) end
    end
    if sugarbee.btnp('y') then
        if effect then report(synth.note_off(9)); effect=false
        else effect=report(synth.note_on(9,660)) end
    end
    if sugarbee.btnp('b') then report(music.stop())
    elseif sugarbee.btnp('a') then
        report(music.play(song,{voices={1,2,3},loop=looping,volume=volumes[level]}))
    end
end
function draw()
    sugarbee.cls(24,30,40)
    sugarbee.print('MUSIC LAB',60,60,100,220,180,3)
    sugarbee.print('State: '..music.state(),60,140,255,255,255,2)
    sugarbee.print('Next start loops: '..tostring(looping),60,190,255,220,120,2)
    sugarbee.print('Music volume: '..volumes[level]..'    Effect: '..tostring(effect),60,240,220,220,220,2)
    sugarbee.print('A / J: play    B / K: stop music',60,310,220,220,220,2)
    sugarbee.print('X / U: volume (0.8, 0.3, mute)    Y / I: effect',60,350,220,220,220,2)
    sugarbee.print('UP / DOWN (W / S): loop on next start',60,390,220,220,220,2)
    sugarbee.print('START / ENTER: host pause / exit',60,430,220,220,220,2)
    sugarbee.print(status,60,510,255,220,120,2)
end
function shutdown()
    music.stop()
    synth.stop(9)
    -- The native session detaches playback before freeing its loaded assets.
end
