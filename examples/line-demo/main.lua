local elapsed=0
function init() end
function update(dt) elapsed=elapsed+dt end
function draw()
    sugarbee.cls(20,25,40)
    sugarbee.print('LINE DEMO - START pauses',40,25,255,255,255,2)
    local reverse=math.floor(elapsed)%2==1
    sugarbee.print('Endpoint order: '..(reverse and 'reversed' or 'forward'),40,65,255,255,255,2)
    local function segment(x1,y1,x2,y2,r,g,b)
        if reverse then sugarbee.line(x2,y2,x1,y1,r,g,b)
        else sugarbee.line(x1,y1,x2,y2,r,g,b) end
    end
    -- Geometry stays fixed as endpoint order changes every second.
    for i=0,12 do
        segment(240,220,60+i*30,120,255,210,80)
        segment(240,220,60+i*30,320,100,200,255)
    end
    for i=0,8 do
        segment(240,220,60,120+i*25,100,255,140)
        segment(240,220,420,120+i*25,255,120,160)
    end
    segment(540,150,1120,150,255,255,255)
    segment(540,180,540,300,255,255,255)
    -- Identical endpoints: four isolated pixels, each inside a marker box.
    for i=0,3 do
        local x=650+i*100
        sugarbee.rect(x-8,232,17,17,80,100,120,false)
        segment(x,240,x,240,255,255,255)
    end
    sugarbee.print('Single pixels inside boxes',600,285,255,255,255,2)
    sugarbee.print('Fixed lines must not change as endpoint order reverses',40,355,255,255,255,2)
    sugarbee.clip(80,420,1120,190)
    sugarbee.camera(math.floor(100+100*math.sin(elapsed)),0)
    sugarbee.sprite_tint(0,0,0,0) -- lines remain opaque and colored
    for i=0,8 do
        local x=i*180
        segment(x,410,x+90,515,255,210,80)
        segment(x+90,515,x,620,100,200,255)
        segment(x,620,x-90,515,100,255,140)
        segment(x-90,515,x,410,255,120,160)
    end
    -- Both endpoints are far outside the viewport. Only visible pixels matter.
    sugarbee.line(-2147483648,515,2147483647,515,255,255,255)
    sugarbee.camera_reset(); sugarbee.clip_reset(); sugarbee.sprite_tint_reset()
    sugarbee.rect(80,420,1120,190,255,255,255,false)
    sugarbee.print('Clipped scrolling wireframe + full-range horizontal line',40,650,255,255,255,2)
end
function shutdown() end
