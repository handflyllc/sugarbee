local elapsed=0
function init() end
function update(dt) elapsed=elapsed+dt end
function draw()
    sugarbee.cls(20,25,40)
    local reverse=math.floor(elapsed)%2==1
    sugarbee.print('TRIANGLE DEMO - START pauses',40,25,255,255,255,2)
    sugarbee.print('Order: '..(reverse and 'reversed' or 'forward')..' - shapes must stay unchanged',40,65,255,255,255,2)
    local function tri(v,r,g,b)
        if reverse then sugarbee.triangle_fill(v[5],v[6],v[3],v[4],v[1],v[2],r,g,b)
        else sugarbee.triangle_fill(v[1],v[2],v[3],v[4],v[5],v[6],r,g,b) end
    end
    for d=0,1 do
        local x=60+d*330
        local a={x,130,x+260,130,d==0 and x+260 or x,310}
        local b={d==0 and x or x+260,130,x+260,310,x,310}
        if reverse then tri(b,80,150,240); tri(a,255,200,70)
        else tri(a,255,200,70); tri(b,80,150,240) end
    end
    sugarbee.rect(720,130,260,180,100,210,140,true)
    sugarbee.print('Two split rectangles and a reference rectangle',60,330,255,255,255,2)
    tri({1080,130,1030,310,1200,250},255,120,170)
    sugarbee.print('Zero-area marker: empty',40,390,255,255,255,2)
    sugarbee.rect(450,380,100,40,180,180,180,false)
    sugarbee.triangle_fill(460,400,500,400,540,400,255,0,0)
    sugarbee.clip(750,375,450,55)
    -- These two huge triangles cover the panel. Their common diagonal is
    -- outside this panel; its solid color must remain stable.
    tri({-2147483648,-2147483648,2147483647,-2147483648,2147483647,2147483647},60,130,85)
    tri({-2147483648,-2147483648,2147483647,2147483647,-2147483648,2147483647},90,80,150)
    sugarbee.clip_reset()
    sugarbee.print('Full-range triangles',750,440,255,255,255,1)
    sugarbee.clip(80,485,1120,155)
    sugarbee.camera(math.floor(100+100*math.sin(elapsed)),0)
    sugarbee.sprite_tint(0,0,0,0)
    for i=0,7 do
        local x=i*200
        tri({x,460,x+170,650,x-30,650},100,210,240)
    end
    sugarbee.camera_reset(); sugarbee.clip_reset(); sugarbee.sprite_tint_reset()
    sugarbee.rect(80,485,1120,155,255,255,255,false)
    sugarbee.print('Clipped scrolling triangles; sprite tint is ignored',40,675,255,255,255,2)
end
function shutdown() end
