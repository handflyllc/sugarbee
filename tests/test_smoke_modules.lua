local install=dofile(arg[1])
local root=arg[2]
for session=1,2 do
    require=install(root)
    runs=nil; nil_runs=nil; false_runs=nil; attempts=nil; break_cycle=nil
    assert(require('empty')==true and require(string.rep('a',235))==5)
    local v=require('value')
    assert(v.runs==1 and v==require('nested.child'))
    assert(require('falsevalue')==false and require('falsevalue')==false and false_runs==1)
    assert(require('nothing')==true and require('nothing')==true and nil_runs==1)
    assert(not pcall(require,'a')); break_cycle=true; assert(require('a')==9)
    assert(not pcall(require,'retry')); assert(require('retry')==v)
    assert(not pcall(require,'../x')); assert(not pcall(require,'missing'))
    assert(not pcall(require,'binary')); assert(not pcall(require,'oversize'))
end
print('Module runner model passed')
