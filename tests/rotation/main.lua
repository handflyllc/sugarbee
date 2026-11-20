local s=sugarbee
local function bad(f,...)
    assert(not pcall(f,...), 'invalid rotation accepted')
end
function init()
    local t=s.texture_rgba(2,2,string.char(255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255))
    for _,angle in ipairs({0,90,180,270,-90,450,12.5,1e300}) do
        assert(select('#',s.sprite_rotated(t,10,10,angle))==0)
        assert(select('#',s.sprite_rotated(t,10,10,angle,-0.5,2.5))==0)
        assert(select('#',s.sprite_region_rotated(t,10,10,0,0,2,2,4,4,angle))==0)
    end
    for _,v in ipairs({false,math.huge,-math.huge,0/0,{}}) do
        bad(s.sprite_rotated,t,0,0,v)
        bad(s.sprite_rotated,t,0,0,30,v,0)
        bad(s.sprite_region_rotated,t,0,0,0,0,2,2,4,4,v)
    end
    bad(s.sprite_rotated,t,0,0)
    bad(s.sprite_rotated,t,0,0,0,1)
    bad(s.sprite_rotated,t,0,0,0,nil,nil)
    bad(s.sprite_rotated,t,0,0,0,1,1,1)
    bad(s.sprite_rotated,t,0.5,0,0)
    bad(s.sprite_rotated,t,2147483648,0,0)
    bad(s.sprite_rotated,t,0,0,30,2147483648,0)
    bad(s.sprite_region_rotated,t,0,0,0,0,2,2,4,4,30,1)
    bad(s.sprite_region_rotated,t,0,0,0,0,2,2,nil,nil,30)
    bad(s.sprite_region_rotated,t,0,0,1,1,2,2,4,4,30)
    bad(s.sprite_region_rotated,t,0,0,0,0,2,2,0,4,30)
    for _,size in ipairs({{4097,1},{1,4097},{2048,2049},{4096,1025}}) do
        for _,angle in ipairs({0,45}) do
            bad(s.sprite_region_rotated,t,2147483647,2147483647,0,0,2,2,size[1],size[2],angle)
        end
    end
    -- Accepted boundaries are culled offscreen without costly allocation.
    s.sprite_region_rotated(t,100000,100000,0,0,2,2,4096,1024,45)
    s.sprite_region_rotated(t,100000,100000,0,0,2,2,2048,2048,45)
    assert(s.texture_free(t))
    bad(s.sprite_rotated,t,0,0,0)
    bad(s.sprite_region_rotated,t,0,0,0,0,2,2,2,2,0)
end
function update() end
function draw() end
