local sprite, x = nil, 0
function init()
    sprite=sugarbee.texture_rgba(8,8,string.rep(string.char(255,200,60,255),64))
end
function update(dt)
    x=(x+180*dt)%1100
    if sugarbee.btnp('y') then
        sugarbee.clip(0,0,0,0)
        error('Intentional clip isolation test')
    end
end
function draw()
    -- The previous draw deliberately leaves an empty clip. This title must
    -- still appear: the host resets clipping before every draw callback.
    sugarbee.cls(20,25,40)
    sugarbee.print('CLIP DEMO - START pauses; Y triggers test Guru',40,30,255,255,255,2)
    sugarbee.rect(160,160,960,400,50,80,100,true)
    sugarbee.clip(160,160,960,400)
    sugarbee.sprite_region(sprite,math.floor(x),120,0,0,8,8,160,480,true,false)
    sugarbee.print('SCROLLING TEXT THROUGH THE PANEL EDGES',math.floor(x)-400,300,255,255,255,3)
    sugarbee.rect(100,500,1080,100,180,80,100,true)
    sugarbee.clip_reset()
    sugarbee.rect(160,160,960,400,255,255,255,false)
    sugarbee.print('Footer: explicit reset. Panel edges must stay clean.',40,620,255,255,255,2)
    sugarbee.clip(0,0,0,0)
    sugarbee.rect(0,0,1280,720,255,0,0,true) -- must be invisible
end
function shutdown() sugarbee.clip(0,0,0,0) end
-- Y/I intentionally raises Guru to check host clip isolation; this demo is
-- excluded from random-input smoke runs because that failure is deliberate.
