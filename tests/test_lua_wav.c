#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#include "../src/sb_audio_sdl.c"
#include "../src/sb_lua_audio.c"
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Lua WAV line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static bool run(const char *source) {
    int top=lua_gettop(g_engine.L);
    if (luaL_dostring(g_engine.L,source)!=LUA_OK) {
        fprintf(stderr,"%s\n",lua_tostring(g_engine.L,-1)); return false;
    }
    return lua_gettop(g_engine.L)==top;
}
static lua_Alloc original_allocator;
static void *allocator_user;
static void *deny_growth(void *ud,void *ptr,size_t old,size_t size) {
    (void)ud;
    if (size>old || (!ptr && size)) return NULL;
    return original_allocator(allocator_user,ptr,old,size);
}
static bool memory_failure(const char *source) {
    lua_State *L=g_engine.L;
    if (luaL_loadstring(L,source)!=LUA_OK) return false;
    original_allocator=lua_getallocf(L,&allocator_user);
    lua_setallocf(L,deny_growth,NULL);
    int status=lua_pcall(L,0,0,0);
    lua_setallocf(L,original_allocator,allocator_user);
    if (status!=LUA_OK) lua_pop(L,1);
    return status==LUA_ERRMEM;
}
static void render(unsigned frames) {
    float out[2048];
    while (frames) {
        unsigned n=frames>1024 ? 1024 : frames;
        audio_output(g_engine.audio,(Uint8 *)out,(int)(n*2*sizeof(float))); frames-=n;
    }
}
static size_t bytes(void) {
    return g_engine.audio ? g_engine.audio->wav.bytes : g_engine.lua_audio->silent.bytes;
}
static bool keypress(Uint32 type,SDL_Scancode code) {
    SDL_Event e={0}; e.type=e.key.type=type; e.key.keysym.scancode=code;
    if (SDL_PushEvent(&e)!=1 || !sb_input_poll(g_engine.input)) return false;
    sb_input_begin_update(g_engine.input);
    call_lua_global_function("update",true,1.0/60);
    sb_input_end_update(g_engine.input);
    call_lua_global_function("draw",false,0);
    return !g_engine.guru_active;
}
int main(int argc,char **argv) {
    CHECK(argc==3); bool unavailable=!strcmp(argv[2],"unavailable");
    if (unavailable) CHECK(SDL_setenv("SDL_AUDIODRIVER","sugarbee-no-device",1)==0);
    CHECK(init_engine()); CHECK((g_engine.audio==NULL)==unavailable);
    if (g_engine.audio) SDL_PauseAudioDevice(g_engine.audio->device,1);
    CHECK(start_cartridge(argv[1])); CHECK(bytes()>0);
    /* Select the looping sample through actual lab input. */
    for (unsigned i=0;i<2;++i) {
        CHECK(keypress(SDL_KEYDOWN,SDL_SCANCODE_S)); CHECK(keypress(SDL_KEYUP,SDL_SCANCODE_S));
    }
    /* Actual lab input, including overlap, pan, volume and stop. */
    CHECK(keypress(SDL_KEYDOWN,SDL_SCANCODE_J)); CHECK(keypress(SDL_KEYUP,SDL_SCANCODE_J));
    CHECK(keypress(SDL_KEYDOWN,SDL_SCANCODE_J)); CHECK(keypress(SDL_KEYUP,SDL_SCANCODE_J));
    CHECK(keypress(SDL_KEYDOWN,SDL_SCANCODE_D)); CHECK(keypress(SDL_KEYUP,SDL_SCANCODE_D));
    CHECK(keypress(SDL_KEYDOWN,SDL_SCANCODE_U)); CHECK(keypress(SDL_KEYUP,SDL_SCANCODE_U));
    set_cartridge_paused(true);
    if (!unavailable) {
        render(480); sb_wav_pool_t frozen=g_engine.audio->wav; render(1024);
        CHECK(!memcmp(&frozen,&g_engine.audio->wav,sizeof frozen));
    }
    set_cartridge_paused(false);
    CHECK(keypress(SDL_KEYDOWN,SDL_SCANCODE_K)); CHECK(keypress(SDL_KEYUP,SDL_SCANCODE_K));
    if (!unavailable) render(480);
    CHECK(run("a=sugarbee.audio; s=assert(a.load('assets/mono.wav')); assert(type(s)=='userdata')"));
    size_t retained=bytes();
    CHECK(memory_failure("return sugarbee.audio.load('assets/mono.wav')"));
    CHECK(bytes()==retained);
    if (!unavailable) {
        sb_wav_pool_t snapshot=g_engine.audio->wav;
        CHECK(memory_failure("return sugarbee.audio.play(s,{loop=true})"));
        CHECK(!memcmp(&snapshot,&g_engine.audio->wav,sizeof snapshot));
    }
    CHECK(run(
        "local function bad(f) assert(not pcall(f)) end "
        "bad(function() a.load(123) end); bad(function() a.load('../x.wav') end); "
        "bad(function() a.load('/etc/passwd') end); bad(function() a.load('assets/mono.wav'..string.char(0)) end); "
        "bad(function() a.load('missing.wav') end); bad(function() a.load('main.lua') end); "
        "bad(function() a.load('assets/mono.wav',true) end); bad(function() a.play({}) end); "
        "bad(function() a.is_playing(s) end); bad(function() a.unload(1) end); "
        "bad(function() a.play(s,{volume='1'}) end); bad(function() a.play(s,{volume=0/0}) end); "
        "bad(function() a.play(s,{pan=math.huge}) end); bad(function() a.play(s,{pan=-2}) end); "
        "bad(function() a.play(s,{loop=0}) end); "
        "bad(function() a.play(s,{oops=1}) end); bad(function() a.play(s,{[1]=1}) end); "
        "bad(function() a.play(s,{['volume'..string.char(0)]=.5}) end); "
        "bad(function() a.play(s,nil) end); bad(function() a.play(s,{},true) end); "
        "assert(getmetatable(s)=='protected audio handle')"));
    CHECK(bytes()==retained);
    if (unavailable) {
        CHECK(run("local p,why=a.play(s,{loop=true}); assert(p==nil and why=='audio_unavailable'); assert(a.unload(s)); assert(not pcall(a.play,s))"));
    } else {
        CHECK(run("p=assert(a.play(s,{volume=.8,pan=-1,loop=false})); assert(a.is_playing(p)); "
            "local ok,why=a.unload(s); assert(not ok and why=='asset_in_use'); "
            "assert(a.set_volume(p,.4)); assert(a.set_pan(p,1)); "
            "assert(not pcall(a.set_volume,p,'0.2')); assert(not pcall(a.set_pan,p,2)); "
            "assert(a.stop(p)); assert(not a.stop(p)); assert(not a.set_volume(p,.2)); "
            "assert(a.is_playing(p)); assert(not a.unload(s))"));
        render(480);
        CHECK(run("assert(not a.is_playing(p)); assert(not a.stop(p)); assert(not a.set_pan(p,0)); "
            "assert(not pcall(a.set_volume,p,2)); "
            "old=p; p=assert(a.play(s)); assert(not a.stop(old)); assert(a.is_playing(p))"));
        render(96000);
        CHECK(run("assert(not a.is_playing(p)); assert(a.unload(s)); assert(not pcall(a.unload,s)); "
            "s=assert(a.load('assets/mono.wav')); slots={}; for i=1,8 do slots[i]=assert(a.play(s)) end; "
            "local p,why=a.play(s); assert(p==nil and why=='voices_full'); "
            "for i=1,8 do assert(a.is_playing(slots[i])) end"));
        render(96000);
        CHECK(run("for i=1,8 do assert(not a.is_playing(slots[i])) end; assert(a.unload(s))"));
    }
    /* Looping uses the same slots and lifecycle as one-shots. */
    if (!unavailable) {
        CHECK(run("s=assert(a.load('assets/loop.wav')); slots={}; "
            "for i=1,8 do slots[i]=assert(a.play(s,{loop=true})) end; "
            "local p,why=a.play(s); assert(p==nil and why=='voices_full')"));
        render(192017);
        CHECK(run("for i=1,8 do assert(a.is_playing(slots[i])) end; "
            "assert(not a.unload(s)); assert(a.set_volume(slots[1],.3)); assert(a.set_pan(slots[1],1))"));
        set_cartridge_paused(true); render(480);
        sb_wav_pool_t frozen=g_engine.audio->wav; render(1024);
        CHECK(!memcmp(&frozen,&g_engine.audio->wav,sizeof frozen));
        set_cartridge_paused(false); render(480);
        CHECK(run("for i=1,8 do assert(a.stop(slots[i])) end"));
        render(480);
        CHECK(run("for i=1,8 do assert(not a.is_playing(slots[i])) end; assert(a.unload(s))"));
    }
    /* Real shutdown can use assets; errors are contained and cleanup follows. */
    CHECK(run("function shutdown() local h=assert(a.load('assets/stereo.wav')); shutdown_ok=true; a.play(h,{loop=true}) end"));
    call_lua_shutdown(); CHECK(run("assert(shutdown_ok)"));
    end_cartridge(); CHECK(!g_engine.lua_audio && !g_engine.L);
    if (!unavailable) CHECK(!g_engine.audio->wav.bytes);
    CHECK(start_cartridge(argv[1]));
    CHECK(run("a=sugarbee.audio; handles={}; for i=1,61 do handles[i]=assert(a.load('assets/mono.wav')) end; "
        "local h,why=a.load('assets/mono.wav'); assert(h==nil and why=='assets_full'); "
        "assert(a.unload(handles[1])); assert(a.load('assets/mono.wav'))"));
    end_cartridge();
    CHECK(start_cartridge(argv[1]));
    CHECK(run("a=sugarbee.audio; for i=1,41 do assert(a.load('assets/stereo.wav')) end; "
        "local h,why=a.load('assets/stereo.wav'); assert(h==nil and why=='audio_budget')"));
    if (!unavailable) CHECK(run("failure_loop=assert(a.play(assert(a.load('assets/loop.wav')),{loop=true}))"));
    /* Guru after assets exist: refuse further acquisition and clean on exit. */
    CHECK(run("function update() error('expected WAV failure') end"));
    call_lua_global_function("update",true,.016); CHECK(g_engine.guru_active);
    CHECK(run("local h,why=a.load('assets/mono.wav'); assert(h==nil and why=='audio_rejected')"));
    end_cartridge(); CHECK(!g_engine.lua_audio);
    if (!unavailable) CHECK(!g_engine.audio->wav.bytes);
    g_engine.guru_active=false;
    CHECK(start_cartridge(argv[1]));
    CHECK(keypress(SDL_KEYDOWN,SDL_SCANCODE_J));
    shutdown_engine(); CHECK(!g_engine.audio && !g_engine.lua_audio);
    puts("Lua WAV and cartridge checks passed");
    return 0;
}
