#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#include "../src/sb_audio_sdl.c"
#include "../src/sb_lua_music.c"
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Lua music line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static bool run(const char *source) {
    int top=lua_gettop(g_engine.L);
    if (luaL_dostring(g_engine.L,source)!=LUA_OK) {
        fprintf(stderr,"%s\n",lua_tostring(g_engine.L,-1)); return false;
    }
    return lua_gettop(g_engine.L)==top;
}
static sb_music_assets_t *pool(void) {
    return g_engine.audio ? &g_engine.audio->music_assets : &g_engine.lua_music->silent;
}
static lua_Alloc allocator;
static void *allocator_user;
static void *deny_growth(void *ud,void *ptr,size_t old,size_t size) {
    (void)ud;
    if (size>old || (!ptr && size)) return NULL;
    return allocator(allocator_user,ptr,old,size);
}
static bool memory_failure(const char *source) {
    lua_State *L=g_engine.L;
    if (luaL_loadstring(L,source)!=LUA_OK) return false;
    allocator=lua_getallocf(L,&allocator_user); lua_setallocf(L,deny_growth,NULL);
    int status=lua_pcall(L,0,0,0);
    lua_setallocf(L,allocator,allocator_user);
    if (status!=LUA_OK) lua_pop(L,1);
    return status==LUA_ERRMEM;
}
static bool close_checked;
static int record_closed(lua_State *L) { close_checked=lua_toboolean(L,1); return 0; }
static void render(unsigned frames) {
    float output[2048];
    while (frames) {
        unsigned n=frames>1024?1024:frames;
        audio_output(g_engine.audio,(Uint8 *)output,(int)(n*2*sizeof(float))); frames-=n;
    }
}
int main(int argc,char **argv) {
    CHECK(argc==3); bool unavailable=!strcmp(argv[2],"unavailable");
    if (unavailable) CHECK(SDL_setenv("SDL_AUDIODRIVER","sugarbee-no-device",1)==0);
    CHECK(init_engine()); CHECK((g_engine.audio==NULL)==unavailable);
    if (g_engine.audio) SDL_PauseAudioDevice(g_engine.audio->device,1);
    CHECK(start_cartridge(argv[1])); CHECK(pool()->bytes>0);
    CHECK(run("m=sugarbee.music; assert(type(song)=='userdata'); assert(getmetatable(song)=='protected music handle'); "
        "local a=m.info(song); a.track_count=99; assert(m.info(song).track_count==3); "
        "local count=0; for _ in pairs(m.info(song)) do count=count+1 end; assert(count==4); "
        "assert(select('#',m.info(song))==1); local s=m.load('assets/one-note.json'); "
        "assert(m.info(s).loop_eligible==false); assert(select('#',m.unload(s))==1)"));
    CHECK(run("local function bad(f) assert(not pcall(f)) end; "
        "bad(function() m.load() end); bad(function() m.load(1) end); "
        "bad(function() m.load('assets/music.json',true) end); "
        "bad(function() m.load('') end); bad(function() m.load('assets/music.json'..string.char(0)) end); "
        "bad(function() m.load('../x') end); bad(function() m.load('/etc/passwd') end); "
        "bad(function() m.info({}) end); bad(function() m.info(song,true) end); "
        "bad(function() m.unload(nil) end); bad(function() m.unload(song,true) end); "
        "bad(function() sugarbee.audio.unload(song) end); "
        "local h,why=m.load('missing.json'); assert(h==nil and type(why)=='string'); "
        "assert(select('#',m.load('missing.json'))==2); "
        "h,why=m.load('assets/invalid.json'); assert(h==nil and why:find('music.load:',1,true)); "
        "h,why=m.load('main.lua'); assert(h==nil and type(why)=='string')"));
    sb_music_assets_t snapshot=*pool();
    CHECK(memory_failure("return sugarbee.music.load('assets/music.json')"));
    CHECK(!memcmp(&snapshot,pool(),sizeof snapshot));
    CHECK(memory_failure("return sugarbee.music.info(song)"));
    CHECK(!memcmp(&snapshot,pool(),sizeof snapshot));
    /* Handles must belong to the closure's cartridge context, even if IDs match. */
    sb_lua_music_t *foreign=sb_lua_music_create(NULL,argv[1]); CHECK(foreign);
    lua_newtable(g_engine.L); sb_lua_music_register(g_engine.L,foreign);
    lua_getfield(g_engine.L,-1,"music"); lua_setglobal(g_engine.L,"foreign"); lua_pop(g_engine.L,1);
    CHECK(run("local h=assert(foreign.load('assets/music.json')); "
        "assert(not pcall(m.info,h)); assert(not pcall(m.unload,h)); assert(not pcall(m.play,h,{voices={1,2,3}})); "
        "assert(not pcall(foreign.info,song)); assert(foreign.unload(h)); foreign=nil"));
    sb_lua_music_end(foreign); /* Retain context through Lua close. */
    CHECK(run("old=assert(m.load('assets/music.json')); alias=old; assert(m.unload(old)); "
        "assert(not pcall(m.info,alias)); assert(not pcall(m.unload,old)); "
        "s=assert(m.load('assets/music.json')); assert(not pcall(m.info,old))"));
    if (!unavailable) {
        /* Native borrowers still share Lua unload protection. */
        lua_getglobal(g_engine.L,"s"); music_handle_t *h=lua_touserdata(g_engine.L,-1);
        sb_music_handle_t id=h->id; lua_pop(g_engine.L,1); unsigned voices[]={1,2,3};
        CHECK(sb_audio_music_play(g_engine.audio,id,voices,3,true)==SB_MUSIC_ASSET_OK); render(50);
        CHECK(run("local ok,why=m.unload(s); assert(ok==false and why=='music_in_use'); assert(m.info(s).track_count==3)"));
        set_cartridge_paused(true); render(480);
        CHECK(run("assert(not m.unload(s)); assert(m.info(s).loop_eligible)"));
        set_cartridge_paused(false); render(480); CHECK(sb_audio_music_stop(g_engine.audio));
        CHECK(run("assert(not m.unload(s))")); render(480);
        CHECK(run("assert(m.unload(s))"));
    } else CHECK(run("assert(m.unload(s))"));
    /* Public playback validation must leave native playback untouched. */
    CHECK(run("assert(m.state()=='idle'); assert(m.stop()); "
        "local ok,r=m.set_volume(.2); assert(ok==false and r=='music_idle'); "
        "function badplay(o) assert(not pcall(m.play,song,o)) end; "
        "badplay(nil); badplay({}); badplay({voices={1,2}}); badplay({voices={1,1,3}}); "
        "badplay({voices={[1]=1,[3]=3}}); badplay({voices={0,2,3}}); "
        "badplay({voices={1,2,10}}); badplay({voices={1,2,'3'}}); "
        "badplay({voices={1,2,3.5}}); badplay({voices={1,2,3},loop=1}); "
        "badplay({voices={1,2,3},volume='1'}); badplay({voices={1,2,3},volume=0/0}); "
        "badplay({voices={1,2,3},volume=math.huge}); badplay({voices={1,2,3},volume=-1}); "
        "badplay({voices={1,2,3},volume=2}); badplay({voices={1,2,3},unknown=true}); "
        "badplay(setmetatable({}, {__index=function() error('must not run') end})); "
        "assert(not pcall(m.play,song)); assert(not pcall(m.play,song,{voices={1,2,3}},1)); "
        "assert(not pcall(m.state,1)); assert(not pcall(m.stop,1)); "
        "assert(not pcall(m.set_volume)); assert(not pcall(m.set_volume,.5,1)); "
        "assert(not pcall(m.set_volume,0/0)); assert(not pcall(m.set_volume,'1')); "
        "assert(m.state()=='idle')"));
    if (unavailable) {
        snapshot=*pool();
        CHECK(run("local ok,r=m.play(song,{voices={1,2,3}}); "
            "assert(ok==false and r=='audio_unavailable'); assert(m.state()=='idle'); assert(m.stop())"));
        CHECK(!memcmp(&snapshot,pool(),sizeof snapshot));
    } else {
        sb_music_sequence_t *seq=&g_engine.audio->music.sequence;
        CHECK(run("opts={voices={3,1,2},volume=0,loop=true}; assert(m.play(song,opts)); "
            "opts.voices[1]=9; assert(m.state()=='playing'); "
            "local ok,r=m.play(song,{voices={1,2,3}}); assert(ok==false and r=='music_busy'); "
            "assert(not sugarbee.synth.note_on(1,440)); assert(not m.unload(song))"));
        CHECK(seq->voice_map[0]==3 && seq->gain==0 && seq->gain_left==0 && seq->loop);
        float muted[1024]; audio_output(g_engine.audio,(Uint8 *)muted,sizeof muted);
        for (size_t i=0;i<1024;++i) CHECK(muted[i]==0);
        CHECK(seq->frame==512);
        CHECK(run("assert(m.set_volume(.5))")); render(120);
        CHECK(seq->gain_left==360 && seq->gain>0 && seq->gain<.5);
        CHECK(run("assert(m.set_volume(.5))")); CHECK(seq->gain_left==360);
        render(360); CHECK(seq->gain==.5);
        /* Exercise a whole iteration, preserving gain and the copied map. */
        render(1488000-992); CHECK(seq->frame==0 && seq->gain==.5 && seq->voice_map[0]==3);
        render(100);
        CHECK(run("assert(sugarbee.synth.note_on(9,660)); assert(m.stop()); assert(m.state()=='finishing'); "
            "assert(not m.unload(song)); assert(m.stop())"));
        render(480); CHECK(run("assert(m.state()=='idle'); assert(m.stop())"));
        CHECK(sb_music_shared_voice_active(&g_engine.audio->music,9));
        CHECK(run("assert(sugarbee.synth.stop(9)); assert(sugarbee.synth.note_on(2,440)); "
            "local ok,r=m.play(song,{voices={1,2,3}}); assert(ok==false and r=='voice_busy'); "
            "assert(m.state()=='idle')"));
        CHECK(seq->reserved==0); CHECK(run("assert(sugarbee.synth.stop(2))")); render(480);
        CHECK(run("short=assert(m.load('assets/one-note.json')); "
            "local ok,r=m.play(short,{voices={1},loop=true}); assert(ok==false and r=='loop_ineligible'); "
            "assert(m.play(short,{voices={1}}))"));
        CHECK(seq->gain==1 && !seq->loop); render(1000);
        CHECK(run("assert(m.state()=='idle'); assert(m.unload(short))"));
        /* Host pause freezes playback; a held start cannot dispatch events. */
        set_cartridge_paused(true); render(480);
        CHECK(run("assert(m.play(song,{voices={1,2,3},volume=.25}))"));
        sb_music_sequence_t frozen=*seq; render(1024); CHECK(!memcmp(&frozen,seq,sizeof frozen));
        CHECK(run("assert(m.state()=='playing'); assert(m.stop()); assert(m.state()=='idle')"));
        set_cartridge_paused(false); render(480); CHECK(seq->state==SB_MUSIC_IDLE);
        CHECK(run("assert(m.play(song,{voices={1,2,3}}))")); render(100);
        set_cartridge_paused(true); render(100);
        CHECK(run("assert(m.stop())")); render(480);
        set_cartridge_paused(false); render(480); CHECK(run("assert(m.state()=='idle')"));
    }
    /* Native invalidation is detected too, rather than trusting nonzero userdata. */
    CHECK(run("s=assert(m.load('assets/music.json'))"));
    lua_getglobal(g_engine.L,"s"); music_handle_t *h=lua_touserdata(g_engine.L,-1);
    if (unavailable) CHECK(sb_music_assets_unload(pool(),h->id,NULL)==SB_MUSIC_ASSET_OK);
    else CHECK(sb_audio_music_unload(g_engine.audio,h->id)==SB_MUSIC_ASSET_OK);
    lua_pop(g_engine.L,1); CHECK(run("assert(not pcall(m.info,s)); assert(not pcall(m.unload,s)); assert(not pcall(m.play,s,{voices={1,2,3}}))"));
    CHECK(run("slots={}; for i=1,7 do slots[i]=assert(m.load('assets/music.json')) end; "
        "local h,why=m.load('assets/music.json'); assert(h==nil and why=='music_capacity'); "
        "assert(m.info(song).track_count==3); assert(m.unload(slots[1])); "
        "slots[1]=assert(m.load('assets/music.json')); for i=1,7 do assert(m.unload(slots[i])) end"));
    /* Host-installed finalizer exercises close ordering. Cartridge-created
     * __gc remains forbidden; install through C without weakening that rule. */
    lua_pushcfunction(g_engine.L,record_closed); lua_setglobal(g_engine.L,"record_closed");
    CHECK(run("closing=assert(m.load('assets/music.json')); "
        "function closing_check() "
        "local h,r=m.load('assets/music.json'); local i,q=m.info(closing); local ok,t=m.unload(closing); "
        "record_closed(h==nil and r=='session_closed' and i==nil and q=='session_closed' and ok==false and t=='session_closed') end"));
    lua_newuserdatauv(g_engine.L,1,0); lua_newtable(g_engine.L);
    lua_getglobal(g_engine.L,"closing_check"); lua_setfield(g_engine.L,-2,"__gc");
    lua_setmetatable(g_engine.L,-2); lua_setglobal(g_engine.L,"guard");
    end_cartridge(); CHECK(close_checked && !g_engine.lua_music && !g_engine.L);
    sb_lua_music_destroy(foreign);
    if (!unavailable) CHECK(!g_engine.audio->music_assets.bytes);
    CHECK(start_cartridge(argv[1])); CHECK(run("m=sugarbee.music; function shutdown() end"));
    trigger_guru_meditation(123,"Expected music test failure","Test only",NULL);
    snapshot=*pool();
    CHECK(run("local h,r=m.load('assets/music.json'); assert(h==nil and r=='session_failed'); "
        "local i,q=m.info(song); assert(i==nil and q=='session_failed'); "
        "local ok,t=m.unload(song); assert(ok==false and t=='session_failed'); local p,r=m.play(song,{voices={1,2,3}}); assert(p==false and r=='session_failed'); assert(not m.stop()); assert(not m.set_volume(.5))"));
    CHECK(!memcmp(&snapshot,pool(),sizeof snapshot));
    end_cartridge(); if (!unavailable) CHECK(!g_engine.audio->music_assets.bytes);
    g_engine.guru_active=false; /* Launcher dismisses Guru before another launch. */
    /* Init failure after load must release everything through the real host. */
    CHECK(start_cartridge(argv[1]));
    CHECK(run("function init() local h=assert(sugarbee.music.load('assets/music.json')); error('intentional init failure') end"));
    call_lua_global_function("init",false,0); CHECK(g_engine.guru_active);
    end_cartridge(); CHECK(!g_engine.lua_music && !g_engine.L);
    if (!unavailable) CHECK(!g_engine.audio->music_assets.bytes);
    g_engine.guru_active=false;
    CHECK(start_cartridge(argv[1])); end_cartridge();
    shutdown_engine();
    puts(unavailable ? "Lua music unavailable-device checks passed" : "Lua music device checks passed");
    return 0;
}
