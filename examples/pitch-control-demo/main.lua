-- Direct game-update-rate frequency edits; no automatic pitch smoothing.
local synth=sugarbee.synth
local waves={"triangle","saw","pulse"}
local modes={"Stepped 220 / 440 / 880 Hz","Sweep 110 to 880 Hz","Sweep A0 to C8"}
local wave,mode,time,hz,playing=1,1,0,220,false
local status="A / J: start"
local function report(ok,why)
    if not ok then status=why or "Audio request failed" end
    return ok
end
local function frequency()
    if mode==1 then return ({220,440,880})[math.floor(time)%3+1] end
    -- Four seconds up, four seconds down; both endpoints included at 60 Hz.
    local phase=(time%8)/4
    local fraction=phase<=1 and phase or 2-phase
    local low,high=110,880
    if mode==3 then low,high=27.5,4186.01 end
    return math.max(low,math.min(high,low*(high/low)^fraction))
end
function init()
    wave,mode,time,hz,playing=1,1,0,220,false
    status="A / J: start"
    report(synth.configure(1,{waveform=waves[wave],attack=.6,decay=.2,
        sustain=.6,release=1,volume=.35,pan=0,pulse_width=.5,
        filter={bypass=true}}))
end
function update(dt)
    if sugarbee.btnp('x') then mode=mode%3+1; time=0 end
    if sugarbee.btnp('y') then
        local next_wave=wave%3+1
        if report(synth.configure(1,{waveform=waves[next_wave]})) then wave=next_wave end
    end
    if playing then time=time+dt end
    hz=frequency()
    if sugarbee.btnp('b') then
        playing=false
        if report(synth.note_off(1)) then status="Released (1 second); A / J restarts" end
    elseif sugarbee.btnp('a') then
        playing=report(synth.note_on(1,hz))
        if playing then status="Playing; pitch edits preserve envelope" end
    elseif playing then
        if not report(synth.set_frequency(1,hz)) then playing=false end
    end
end
function draw()
    sugarbee.cls(24,30,40)
    sugarbee.print('DIRECT PITCH CONTROL',40,50,100,220,180,3)
    sugarbee.print(waves[wave]..' | '..modes[mode],40,130,255,255,255,2)
    sugarbee.print(string.format('Frequency: %.2f Hz',hz),40,190,255,220,120,3)
    sugarbee.print('A / J: start or retrigger   B / K: release',40,280,255,255,255,2)
    sugarbee.print('X / U: pattern   Y / I: waveform (live)',40,330,255,255,255,2)
    sugarbee.print('ENTER: pause / exit',40,380,255,255,255,2)
    sugarbee.print('Pitch changes run at 60 updates/sec; no pitch ramp.',40,450,200,200,200,2)
    sugarbee.print(status,40,520,255,220,120,2)
end
