local install=dofile(arg[1])
local root=arg[2]
for session=1,2 do
    local read=install(root)
    assert(read('assets/bytes.bin')==string.char(0,1,255,10,0,13))
    assert(select('#',read('one'))==2)
    for _,path in ipairs({'','/x','../x','a/../x','a//x','a/./x','a/','a:b','a'..string.char(92)..'b',
                         'one'..string.char(0),string.rep('a',240)}) do assert(not pcall(read,path)) end
    assert(not pcall(read)); assert(not pcall(read,1)); assert(not pcall(read,'one',nil))
    assert(read(string.rep('a',239))=='long path')
    local a,e=read('absent'); assert(a==nil and e=='not_found')
    a,e=read('oversized'); assert(a==nil and e=='too_large')
    read=install(root)
    local kept=read('large.bin')
    for i=2,16 do assert(#read('large.bin')==1048576) end
    collectgarbage()
    a,e=read('large.bin'); assert(a==nil and e=='too_large' and #kept==1048576)
    assert(read('empty')=='')
    read=install(root)
    for i=1,15 do assert(#read('large.bin')==1048576) end
    assert(read('one')=='q')
    a,e=read('large.bin'); assert(a==nil and e=='too_large')
    assert(#read('almost')==1048575)
end
print('Data runner model passed')
