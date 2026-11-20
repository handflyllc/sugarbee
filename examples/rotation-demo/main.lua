local whole,sheet
local angle,animate=0,false
local function pixels(w,h,cell)
    local out={}
    for y=0,h-1 do for x=0,w-1 do
        local u,v=x%cell,y%cell
        local shaft=u>=cell/8 and u<cell*5/8 and v>=cell*3/8 and v<cell*5/8
        local head=u>=cell/2 and u<cell*7/8 and math.abs(v-cell/2)<=cell*7/8-u
        local lit=shaft or head
        local second=x>=cell
        out[#out+1]=string.char(second and 255 or 80,second and 170 or 220,second and 40 or 255,lit and 255 or 0)
    end end
    return table.concat(out)
end
function init()
    whole=sugarbee.texture_rgba(64,64,pixels(64,64,64))
    sheet=sugarbee.texture_rgba(32,16,pixels(32,16,16))
    angle,animate=0,false
end
function update(dt)
    if sugarbee.btnp('a') then angle=(angle+45)%360 end
    if sugarbee.btnp('b') then animate=not animate end
    if animate then angle=(angle+40*dt)%360 end
end
local function marker(x,y)
    sugarbee.line(x-7,y,x+7,y,255,255,255)
    sugarbee.line(x,y-7,x,y+7,255,255,255)
end
function draw()
    sugarbee.cls(20,30,48)
    sugarbee.print('A/J: +45 degrees   B/K: animate   ENTER: pause',30,25,255,255,255,2)
    sugarbee.print(string.format('Clockwise angle: %.1f',angle),30,65,255,220,80,2)
    sugarbee.print('Whole sprite: center',70,135,255,255,255,1)
    sugarbee.sprite_rotated(whole,128,208,angle)
    marker(160,240)
    sugarbee.print('Sheet region: scaled center',360,135,255,255,255,1)
    sugarbee.sprite_region_rotated(sheet,416,192,16,0,16,16,96,96,angle)
    marker(464,240)
    sugarbee.print('External pivot',690,135,255,255,255,1)
    sugarbee.sprite_region_rotated(sheet,780,192,16,0,16,16,96,96,angle,-36,48)
    marker(744,240)
    sugarbee.print('Tint + opacity',990,135,255,255,255,1)
    sugarbee.sprite_tint(160,255,120,128)
    sugarbee.sprite_region_rotated(sheet,1024,192,0,0,16,16,96,96,angle)
    sugarbee.sprite_tint_reset()
    sugarbee.print('Unrotated reference',80,395,255,255,255,1)
    sugarbee.sprite(whole,128,440)
    sugarbee.print('Clip + camera',430,395,255,255,255,1)
    sugarbee.rect(449,429,162,122,255,255,255,false)
    sugarbee.clip(450,430,160,120)
    sugarbee.camera(10,20)
    sugarbee.sprite_region_rotated(sheet,450,440,0,0,16,16,160,160,angle)
    sugarbee.camera_reset()
    sugarbee.clip_reset()
    sugarbee.print('0: right   90: down   180: left   270: up',30,620,255,255,255,2)
    sugarbee.print('White crosses mark fixed pivots; this text stays unrotated.',30,660,255,255,255,1)
end
function shutdown()
    if whole then sugarbee.texture_free(whole) end
    if sheet then sugarbee.texture_free(sheet) end
end
