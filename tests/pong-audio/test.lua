-- Test-only upvalue access sets precise contacts without adding game debug APIs.
local function session(unavailable)
    local events={}
    local env=setmetatable({}, {__index=_G})
    env.math=setmetatable({random=function() return 1 end}, {__index=math})
    env.print=function() end
    env.sugarbee={btn=function() return false end,btnp=function() return false end,
        cls=function() end,rect=function() end,print=function() end,synth={}}
    for _,name in ipairs({'configure','note_on','note_off','stop'}) do
        env.sugarbee.synth[name]=function(v,arg)
            events[#events+1]={name,v,arg}
            return not unavailable, unavailable and 'audio_unavailable' or nil
        end
    end
    assert(loadfile(arg[1],'t',env))(); env.init()
    assert(#events==2 and events[1][1]=='configure' and events[2][1]=='configure')
    local function value(name,new)
        for i=1,100 do
            local key,v=debug.getupvalue(env.update,i)
            if not key then break end
            if key==name then
                if new~=nil then debug.setupvalue(env.update,i,new) end
                return v
            end
        end
        error('missing game state '..name)
    end
    local function place(x,y,vx,vy)
        value('ball_x',x); value('ball_y',y)
        value('ball_velocity_x',vx); value('ball_velocity_y',vy)
    end
    return env,events,value,place
end
for _,silent in ipairs({false,true}) do
    local e,events,get,place=session(silent)
    local function hit(x,y,vx,vy,voice,hz)
        place(x,y,vx,vy)
        local before=#events
        e.update(1/60)
        assert(#events==before+1)
        local last=events[#events]
        assert(last[1]=='note_on' and last[2]==voice and last[3]==hz)
    end
    hit(600,703,200,240,1,330)
    assert(get('ball_y')==704 and get('ball_velocity_y')==-240)
    -- A normal next frame must not retrigger the wall sound.
    local count=#events; e.update(1/60); assert(#events==count)
    e.update(1/60)
    if not silent then assert(events[#events][1]=='note_off') end
    hit(600,1,200,-240,1,330)
    assert(get('ball_y')==0 and get('ball_velocity_y')==240)
    e.update(.1) -- Drain gate before the next isolated contact.
    hit(81,340,-200,240,2,660)
    assert(get('ball_x')==80 and get('ball_velocity_x')==200 and get('ball_velocity_y')==240)
    e.update(.1)
    hit(1183,340,200,-240,2,660)
    assert(get('ball_x')==1184 and get('ball_velocity_x')==-200 and get('ball_velocity_y')==-240)
    e.update(.1)
    -- A miss scores normally without a paddle note.
    place(1263,100,200,240); count=#events
    e.update(1/60)
    assert(#events==count and get('left_score')==1)
    assert(get('ball_x')==632 and get('ball_y')==352)
    -- A corner can trigger both independent voices in the same update.
    get('paddle_y',0); place(81,1,-200,-240); count=#events
    e.update(1/60)
    assert(#events==count+2 and events[count+1][2]==1 and events[count+2][2]==2)
    -- Gates still release on the winner screen, where physics is paused.
    get('winner','LEFT'); e.update(.1)
    if not silent then
        assert(events[#events-1][1]=='note_off' and events[#events][1]=='note_off')
    end
    e.shutdown()
    assert(events[#events-1][1]=='stop' and events[#events-1][2]==1)
    assert(events[#events][1]=='stop' and events[#events][2]==2)
end
print('Pong collision audio checks passed')
