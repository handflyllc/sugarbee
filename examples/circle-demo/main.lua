local elapsed=0
function init() end
function update(dt) elapsed=elapsed+dt end
function draw()
    sugarbee.cls(20,25,40)
    sugarbee.print('CIRCLE DEMO - START pauses',40,25,255,255,255,2)
    sugarbee.print('Outline / fill / fill + outline',40,65,255,255,255,2)
    sugarbee.circle(210,200,95,255,210,80)
    sugarbee.circle_fill(530,200,95,70,140,220)
    sugarbee.circle_fill(850,200,95,70,140,220)
    sugarbee.circle(850,200,95,255,210,80)
    sugarbee.print('Small radii 0 to 8: outline above, fill below',40,315,255,255,255,2)
    for radius=0,8 do
        local x=80+radius*65
        sugarbee.circle(x,365,radius)
        sugarbee.circle_fill(x,400,radius,100,220,255)
    end
    -- Huge fill covers this panel. Its outline is entirely offscreen and must
    -- not add an edge at the clipping rectangle.
    sugarbee.clip(750,340,450,80)
    sugarbee.circle_fill(0,0,2147483647,45,85,65)
    sugarbee.circle(0,0,2147483647,255,0,0)
    sugarbee.clip_reset()
    sugarbee.print('Huge disk: green, no red',750,435,255,255,255,1)
    sugarbee.clip(80,485,1120,155)
    sugarbee.camera(math.floor(100+100*math.sin(elapsed)),0)
    sugarbee.sprite_tint(0,0,0,0) -- primitives stay opaque
    for i=0,7 do
        local x=i*200
        sugarbee.circle_fill(x,560,90,50,100,170)
        sugarbee.circle(x,560,90,255,210,80)
    end
    sugarbee.camera_reset(); sugarbee.clip_reset(); sugarbee.sprite_tint_reset()
    sugarbee.rect(80,485,1120,155,255,255,255,false)
    sugarbee.print('Clipped scrolling disks: no artificial edges at the clip',40,675,255,255,255,2)
end
function shutdown() end
