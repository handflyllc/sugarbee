-- Exercise the smoke model's measurement contract without relying on drawing.
local m=sugarbee.measure_text
local function check(text, scale, w, h)
    local a,b=m(text,scale)
    assert(a==w and b==h)
    assert(select('#',m(text,scale))==2)
end
check('',nil,0,0)
check('A ',nil,16,8)
check('A\n',2,16,36)
check('\n\n',1,0,28)
check('x'..string.char(0)..'y',1,8,8)
check('\t\r\195\169',1,32,8)
check(123,1,24,8)
check('x',0,0,0)
check('x',-2147483648,0,0)
check('x',268435455,2147483640,2147483640)
check(string.rep(' ',262144),1,2097152,8)
assert(not pcall(m,string.rep(' ',262145)))
assert(not pcall(m,'x',268435456))
assert(not pcall(m,'xx',268435455))
assert(not pcall(m,'\n',268435455))
assert(not pcall(m))
assert(not pcall(m,nil))
assert(not pcall(m,true))
assert(not pcall(m,{}))
assert(not pcall(m,'x',1,2))
for _,v in ipairs({false,0.5,math.huge,2147483648,-2147483649}) do
    assert(not pcall(m,'x',v))
end
function draw() end
