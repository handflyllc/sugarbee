-- Behavioral score/timing checks, independent of audio hardware.
local source = assert(arg[1])
local function session(silent)
    local events, buttons, text = {}, {}, {}
    local now = 0
    local env = setmetatable({}, {__index=_G})
    local function record(kind, hz)
        events[#events+1] = {kind=kind,hz=hz,time=now}
        if silent then return false, "audio_unavailable" end
        return true
    end
    env.sugarbee = {
        synth = {
            configure=function() return true end,
            note_on=function(v,hz) assert(v==1 and hz>=110 and hz<=880); return record("on",hz) end,
            note_off=function(v) assert(v==1); return record("off") end,
            stop=function(v) assert(v==1); return record("stop") end
        },
        btnp=function(b) return buttons[b] or false end,
        cls=function() end,
        print=function(s) text[#text+1]=s end
    }
    assert(loadfile(source,"t",env))()
    env.init()
    local function step(dt, a, b)
        now=now+dt
        buttons={a=a,b=b}
        env.update(dt)
        text={}; env.draw()
    end
    return {events=events, step=step, shutdown=env.shutdown,
        display=function() return table.concat(text,"\n") end}
end
local function notes(s)
    local out={}
    for _,e in ipairs(s.events) do if e.kind=="on" then out[#out+1]=e end end
    return out
end
local s=session(false)
s.step(1)
assert(#s.events==0, "launch must be silent")
s.step(0,true)
local start=notes(s)[1].time
for _=1,480 do s.step(1/60) end
local n=notes(s)
local pitches={67,62,67,62,67,62,67,71,74,72,69,72,69,72,69,66,69,62}
local beats={0,1.5,2,3.5,4,4.5,5,5.5,6,8,9.5,10,11.5,12,12.5,13,13.5,14}
assert(#n==18)
for i,e in ipairs(n) do
    assert(math.abs(e.hz-440*2^((pitches[i]-69)/12))<1e-8)
    assert(math.abs(e.time-start-beats[i]/2)<1/60+1e-8, "timing drift")
end
assert(s.display():find("Finished",1,true))
assert(s.events[#s.events].kind=="off")
local count=#s.events
s.step(10); assert(#s.events==count, "must not loop")
s.step(0,true); assert(#notes(s)==19)
s.step(.1); s.step(0,false,true)
count=#s.events; s.step(2); assert(#s.events==count and s.display():find("Stopped"))
s.step(0,true,true); assert(s.events[#s.events].kind=="stop", "stop must win")
s.step(0,true); s.shutdown(); assert(s.events[#s.events].kind=="stop")
s=session(false); s.step(0,true); s.step(.1)
s.step(.1); assert(#notes(s)==1)
-- A delayed update skips passed notes and plays only the current event.
s.step(3); assert(#notes(s)==2)
s.step(100); assert(#notes(s)==2 and s.display():find("Finished"))
-- Uneven timesteps still finish at eight seconds, with no cumulative drift.
s=session(false); s.step(0,true)
for _=1,80 do s.step(.037); s.step(.063) end
assert(#notes(s)==18 and s.display():find("Finished"))
s=session(true); s.step(0,true)
for _=1,480 do s.step(1/60) end
assert(#notes(s)==18 and s.display():find("Audio unavailable"))
s.step(0,false,true); assert(s.events[#s.events].kind=="stop")
print("Musical Synth Lab timing checks passed")
