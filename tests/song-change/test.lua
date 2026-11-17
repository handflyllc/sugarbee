local root=assert(arg[1])
local services=dofile(root..'/tests/smoke_services.lua')
for _,unavailable in ipairs({false,true}) do
    local config=dofile(root..'/tests/song-change/scenario.lua')
    config.audio_available=not unavailable
    local api,control=services.new(config,function() return true end)
    sugarbee=api
    sugarbee.btnp=function() return false end
    test_advance=control.advance; test_unavailable=unavailable
    dofile(root..'/examples/song-change/main.lua'); control.begin(); init()
    dofile(root..'/tests/song-change/checks.lua')
    shutdown()
end
-- Loading failures are handled without turning a missing song into a play call.
local api,control=services.new({},function() return false end)
sugarbee=api; sugarbee.btnp=function() return false end
dofile(root..'/examples/song-change/main.lua'); control.begin(); init()
assert(not request_song('A')); update()
assert(song_change.current==nil and song_change.error=='Asset not found')
print('Song change smoke-model policy checks passed')
