-- Trusted-directory model, not a ZIP validator or host filesystem sandbox.
return function(root)
    local charged=0
    return function(...)
        local path=...
        assert(select('#',...)==1 and type(path)=='string', 'read_file: expected one path string')
        assert(#path>=1 and #path<=239 and not path:find('[%z\1-\31\127\\:]')
            and path:sub(1,1)~='/', 'read_file: invalid cartridge-relative path')
        for segment in (path..'/'):gmatch('([^/]*)/') do
            assert(segment~='' and segment~='.' and segment~='..', 'read_file: invalid path segment')
        end
        local file,_,code=io.open(root..'/'..path,'rb')
        if not file then return nil,code==2 and 'not_found' or 'io_error' end
        local size=file:seek('end')
        if not size then file:close(); return nil,'io_error' end
        if size>1048576 or size>16777216-charged then file:close(); return nil,'too_large' end
        if not file:seek('set',0) then file:close(); return nil,'io_error' end
        local bytes=size==0 and '' or file:read(size)
        charged=charged+(bytes and #bytes or 0)
        local closed=file:close()
        if not bytes or #bytes~=size or not closed then return nil,'io_error' end
        return bytes,nil
    end
end
