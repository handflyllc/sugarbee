-- Workload is a game-author choice; no diagnostic API is needed.
local level,mode,phase,result=1,1,0,0
local counts={50,400,1600}
function init() level,mode,phase,result=1,1,0,0 end
function update(dt)
    if sugarbee.btnp('a') then level=level%3+1 end
    if sugarbee.btnp('b') then mode=mode%2+1 end
    phase=(phase+dt*40)%1000
    if mode==2 then
        local sum=0
        for i=1,counts[level]*3 do sum=sum+math.sin(i+phase) end
        result=sum
    end
end
function draw()
    sugarbee.cls(20,30,48)
    sugarbee.print('A/J: cycle load   B/K: draw or update work   ENTER: pause',30,100,255,255,255,2)
    sugarbee.print(string.format('Level %d / 3   Objects %d   Extra work: %s',level,counts[level],mode==1 and 'draw' or 'update'),30,140,255,220,80,2)
    sugarbee.print('Compare Upd / Draw; cycle back to level 1 to reduce work.',30,180,255,255,255,2)
    for i=1,counts[level] do
        local x=math.floor((i*37+phase)%1240)
        local y=240+(i*19)%440
        sugarbee.rect(x,y,12,12,80,180,240,true)
        if mode==1 then
            sugarbee.rect(x+2,y+2,8,8,255,180,60,false)
        end
    end
    sugarbee.print(string.format('Update result: %.2f',result),30,210,255,255,255,1)
end
