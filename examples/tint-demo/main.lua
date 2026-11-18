local sheet, elapsed = nil, 0
local colors={{255,90,90},{90,255,140},{100,160,255},{255,210,80}}
function init()
    -- Two original 8x8 glyphs in one white/gray sheet, transparent background.
    local a={'  ####  ',' ##  ## ','##    ##','##    ##','########','##    ##','##    ##','        '}
    local diamond={'   ##   ','  ####  ',' ###### ','########',' ###### ','  ####  ','   ##   ','        '}
    local pixels={}
    for y=1,8 do
        for glyph=1,2 do
            local row=(glyph==1 and a or diamond)[y]
            for x=1,8 do
                local lit=row:sub(x,x)=='#'
                local shade=(glyph==2 and y>=5) and 128 or 255
                pixels[#pixels+1]=string.char(shade,shade,shade,lit and 255 or 0)
            end
        end
    end
    sheet=sugarbee.texture_rgba(16,8,table.concat(pixels))
end
function update(dt)
    elapsed=elapsed+dt
    if sugarbee.btnp('y') then
        sugarbee.sprite_tint(0,0,0,0)
        error('Intentional sprite tint isolation test')
    end
end
function draw()
    sugarbee.cls(20,25,40)
    sugarbee.print('SPRITE TINT - START pauses; Y triggers test Guru',40,25,255,255,255,2)
    -- Previous draw leaves opacity zero: this must appear in original colors.
    sugarbee.sprite_region(sheet,40,75,0,0,16,8,128,64)
    sugarbee.print('Frame reset: white / gray',200,95,255,255,255,2)
    for i,c in ipairs(colors) do
        sugarbee.sprite_tint(c[1],c[2],c[3])
        sugarbee.sprite_region(sheet,40+(i-1)*220,180,0,0,16,8,128,64)
    end
    sugarbee.sprite_tint_reset()
    sugarbee.sprite_region(sheet,960,180,0,0,16,8,128,64)
    sugarbee.print('Reset',960,255,255,255,255,2)
    sugarbee.print('One shared sheet; gray stays darker; transparent holes stay clear',40,290,255,255,255,2)
    for row=0,3 do for col=0,7 do
        local c=(row+col)%2==0 and 60 or 100
        sugarbee.rect(40+col*16,340+row*16,16,16,c,c,c,true)
    end end
    local alpha=math.floor(127.5+127.5*math.sin(elapsed*1.5))
    sugarbee.sprite_tint(100,200,255,alpha)
    sugarbee.sprite_region(sheet,40,340,0,0,16,8,128,64)
    sugarbee.print('Opacity '..alpha..' - text and rectangles stay opaque',200,360,255,255,255,2)
    -- Camera and clip remain independent of tint.
    sugarbee.clip(40,460,1120,100)
    sugarbee.camera(math.floor(elapsed*80)%160,0)
    for i=0,8 do
        local c=colors[i%4+1]
        sugarbee.sprite_tint(c[1],c[2],c[3])
        sugarbee.sprite_region(sheet,40+i*160,470,8,0,8,8,64,64,i%2==0,true)
    end
    sugarbee.camera_reset(); sugarbee.clip_reset(); sugarbee.sprite_tint_reset()
    sugarbee.rect(40,460,1120,100,255,255,255,false)
    sugarbee.print('Clipped, scrolling, flipped glyphs above. Fixed HUD below.',40,620,255,255,255,2)
    sugarbee.sprite_tint(0,0,0,0)
end
function shutdown() sugarbee.sprite_tint(0,0,0,0) end
-- Intentional Y/I errors exclude this demo from random-input smoke runs.
