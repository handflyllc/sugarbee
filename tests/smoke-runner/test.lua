-- Deterministic model checks, separate from real native API tests.
local services = dofile(assert(arg[1]))
local config={songs={a={track_count=2,end_frame=1600,loop_eligible=true},
                     b={track_count=1,end_frame=0,loop_eligible=false}}}
local function exists(path) return path~='missing' end
local s,c=services.new(config,exists)
local m=s.music
local function result(expected, reason, ...)
    assert(select('#',...)==2)
    local a,b=...; assert(a==expected and b==reason)
end
local function bad(fn, ...)
    assert(not pcall(fn, ...))
end
result(nil,'io_error',s.load_save())
result(nil,'io_error',s.save('top-level'))
result(nil,'io_error',s.delete_save())
c.begin()
result(nil,'not_found',s.load_save())
for _,bytes in ipairs({'', 'a\0b', string.rep('x',65536)}) do
    result(true,nil,s.save(bytes)); result(bytes,nil,s.load_save())
end
result(nil,'too_large',s.save(string.rep('x',65537)))
result(string.rep('x',65536),nil,s.load_save())
bad(s.save); bad(s.save,12); bad(s.save,'ok',nil); bad(s.load_save,nil)
bad(s.delete_save,nil)
result(true,nil,s.delete_save()); result(nil,'not_found',s.load_save())
result(true,nil,s.delete_save()); result(true,nil,s.save('after deletion'))
result('after deletion',nil,s.load_save())
local other, oc=services.new(config,exists); oc.begin()
result(nil,'not_found',other.load_save()) -- per-run isolation
result(nil,'Asset not found',m.load('missing'))
bad(m.load,'no metadata'); bad(m.load,2); bad(m.load,'a',nil)
local a,b=m.load('a'),m.load('b')
local info=m.info(a); info.track_count=9; assert(m.info(a).track_count==2)
bad(other.music.info,a); bad(m.info,{})
result(false,'music_idle',m.set_volume(0))
for _,opts in ipairs({{}, {voices={1}}, {voices={1,1}}, {voices={[2]=1,[3]=2}},
                      {voices={1,10}}, {voices={1,2},extra=true},
                      {voices={1,2},loop=1}, {voices={1,2},volume=0/0}}) do
    bad(m.play,a,opts); assert(m.state()=='idle')
end
result(false,'loop_ineligible',m.play(b,{voices={1},loop=true}))
assert(m.play(a,{voices={2,1},volume=0}))
result(false,'music_busy',m.play(b,{voices={3}}))
result(false,'music_in_use',m.unload(a))
assert(m.set_volume(1)); c.advance(); assert(m.state()=='playing')
c.advance(); assert(m.state()=='finishing')
result(false,'music_busy',m.play(a,{voices={1,2}}))
c.advance(); assert(m.state()=='idle')
assert(m.play(a,{voices={1,2},loop=true}))
for _=1,10 do c.advance() end
assert(m.state()=='playing')
assert(m.stop()); assert(m.stop()); assert(m.state()=='finishing')
result(false,'music_in_use',m.unload(a))
c.advance(); assert(m.state()=='idle')
assert(m.play(a,{voices={1,2}})) -- same asset can restart after synthetic fade
assert(m.stop()); c.advance(); assert(m.unload(a)); bad(m.info,a); bad(m.play,a,{voices={1,2}})
local handles={b}
for i=2,8 do handles[i]=m.load('a') end
result(nil,'music_capacity',m.load('a'))
assert(m.unload(handles[2])); assert(m.load('a'))
local noaudio=services.new({songs=config.songs,audio_available=false},exists)
local h=noaudio.music.load('a')
result(false,'audio_unavailable',noaudio.music.play(h,{voices={1,2}}))
assert(noaudio.music.state()=='idle' and noaudio.music.stop())
assert(noaudio.music.unload(h))
local ok,err=pcall(function() return m.seek end)
assert(not ok and err:find('unsupported API sugarbee.music.seek',1,true))
print('smoke service model checks passed')
