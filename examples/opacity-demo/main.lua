local levels = {0, 64, 128, 192, 255}
local selected = 3
function init() end
function update()
    if sugarbee.btnp('a') then selected = selected % #levels + 1 end
end
function draw()
    sugarbee.cls(25, 40, 70)
    for y=100,580,40 do
        for x=40,1200,40 do
            if (x+y)%80==20 then sugarbee.rect(x,y,40,40,70,110,150,true) end
        end
    end
    local a = levels[selected]
    sugarbee.print('A/J: cycle opacity   ENTER: pause   alpha = '..a,40,30,255,255,255,2)
    sugarbee.rect(80,140,500,170,0,0,0,true,a)
    sugarbee.print('Opaque label on fading panel',100,180,255,255,255,1)
    sugarbee.rect(660,140,480,170,255,220,80,false,a)
    sugarbee.print('Outline: uniform corners',680,190,255,220,80,1,a)
    sugarbee.print('Fading text\nTwo lines at scale 3',80,360,255,255,255,3,a)
    sugarbee.clip(690,370,380,90)
    sugarbee.camera(10,10)
    sugarbee.rect(680,360,430,130,220,60,40,true,a)
    sugarbee.print('Clipped + camera',720,400,255,255,255,2,a)
    sugarbee.camera_reset()
    sugarbee.clip_reset()
    sugarbee.rect(40,620,1200,60,0,0,0,true)
    sugarbee.print('This footer stays opaque at every opacity setting.',60,640,255,255,255,2)
end
function shutdown() end
