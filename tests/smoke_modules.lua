-- Trusted-directory module model. No native ZIP/CRC checks or instruction budget.
return function(root)
    local cache, loading, stack = {}, {}, {}
    local charged, count = 0, 0
    local function require_module(...)
        local name=...
        assert(select('#',...)==1 and type(name)=='string',
               'require: expected one module-name string')
        assert(#name>=1 and #name<=235, 'require: module name must contain 1..235 bytes')
        assert(not name:find('[^A-Za-z0-9_.]') and not name:find('..',1,true)
            and name:sub(1,1)~='.' and name:sub(-1)~='.', 'require: invalid module name')
        for segment in name:gmatch('[^.]+') do
            assert(segment:match('^[A-Za-z_][A-Za-z0-9_]*$'), 'require: invalid module name')
        end
        if cache[name]~=nil then return cache[name] end
        if loading[name] then error('require: circular import: '..table.concat(stack,' -> ')..' -> '..name) end
        assert(#stack<16, 'require: nested import limit is 16')
        assert(count+#stack<128, 'require: module cache limit is 128 (including pending loads)')
        local member=name:gsub('%.','/')..'.lua'
        local file,why=io.open(root..'/'..member,'rb')
        if not file then error("require '"..name.."': "..tostring(why)) end
        local size=file:seek('end')
        if not size or size>262144 or size>8388608-charged then
            file:close(); error("require '"..name.."': source limit exceeded")
        end
        file:seek('set',0)
        local source=size==0 and '' or file:read(size)
        file:close()
        if not source then error("require '"..name.."': source read failed") end
        charged=charged+#source
        local chunk,err=load(source,'@'..member,'t',_G)
        if not chunk then error("require '"..name.."': "..err) end
        stack[#stack+1]=name; loading[name]=true
        local ok,value=pcall(chunk)
        stack[#stack]=nil; loading[name]=nil
        if not ok then error(value,0) end
        if value==nil then value=true end
        cache[name]=value; count=count+1
        return value
    end
    return require_module
end
