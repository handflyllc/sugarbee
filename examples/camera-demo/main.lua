local sprite, elapsed = nil, 0
function init()
    sprite=sugarbee.texture_rgba(8,8,string.rep(string.char(255,200,60,255),64))
end
function update(dt)
    elapsed=elapsed+dt
    if sugarbee.btnp('y') then
        sugarbee.camera(2147483647,-2147483648)
        sugarbee.clip(0,0,0,0)
        error('Intentional camera isolation test')
    end
end
function draw()
    -- Previous draw leaves an extreme camera and empty clip. Host resets both.
    sugarbee.cls(20,25,40)
    sugarbee.print('CAMERA DEMO - START pauses; Y triggers test Guru',40,30,255,255,255,2)
    sugarbee.rect(160,160,960,400,50,80,100,true)
    sugarbee.clip(160,160,960,400)
    local world_left=math.floor(400+400*math.sin(elapsed*0.4))
    local world_top=math.floor(160+160*math.sin(elapsed*0.3))
    -- The camera maps world_left/top to the panel's screen origin (160,160).
    sugarbee.camera(world_left-160,world_top-160)
    for row=0,9 do
        for col=0,15 do
            local x,y=col*120,row*100
            sugarbee.rect(x,y,110,90,80,130,160,false)
            sugarbee.sprite_region(sprite,x+8,y+8,0,0,8,8,24,24,col%2==0,false)
            sugarbee.print(col..','..row,x+8,y+50,255,255,255,1)
        end
    end
    sugarbee.camera_reset()
    sugarbee.clip_reset()
    sugarbee.rect(160,160,960,400,255,255,255,false)
    sugarbee.print('Fixed HUD: world origin '..world_left..','..world_top,40,620,255,255,255,2)
    sugarbee.camera(2147483647,-2147483648)
    sugarbee.clip(0,0,0,0)
end
function shutdown()
    sugarbee.camera(2147483647,-2147483648)
    sugarbee.clip(0,0,0,0)
end
-- Intentional Y/I errors exclude this demo from random-input smoke runs.
