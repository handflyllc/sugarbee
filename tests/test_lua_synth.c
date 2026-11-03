/* Real host registration and SDL callback; dummy output paused for inspection. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#include "../src/sb_audio_sdl.c"
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Lua synth line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static bool run(const char *source) {
    int top=lua_gettop(g_engine.L);
    if (luaL_dostring(g_engine.L,source)!=LUA_OK) {
        fprintf(stderr,"%s\n",lua_tostring(g_engine.L,-1));
        return false;
    }
    return lua_gettop(g_engine.L)==top;
}
static bool fresh(sb_audio_t *audio) {
    g_engine.audio=audio;
    sb_audio_session_begin(audio);
    g_engine.L=luaL_newstate();
    if (!g_engine.L) return false;
    open_cartridge_libraries(g_engine.L);
    register_sugarbee_api(g_engine.L);
    g_engine.cart_loaded=true;
    g_engine.cart_initialized=true;
    g_engine.guru_active=false;
    return true;
}
static bool lab_key(Uint32 type, SDL_Scancode key) {
    SDL_Event event={0};
    event.type=event.key.type=type;
    event.key.keysym.scancode=key;
    if (SDL_PushEvent(&event)!=1 || !sb_input_poll(g_engine.input)) return false;
    sb_input_begin_update(g_engine.input);
    call_lua_global_function("update",true,1.0/60);
    sb_input_end_update(g_engine.input);
    call_lua_global_function("draw",false,0);
    return !g_engine.guru_active;
}
static int lab(const char *path, bool unavailable) {
    if (unavailable) CHECK(SDL_setenv("SDL_AUDIODRIVER","sugarbee-no-device",1)==0);
    CHECK(init_engine());
    sb_audio_t *a=g_engine.audio;
    CHECK((a==NULL)==unavailable);
    if (a) SDL_PauseAudioDevice(a->device,1);
    CHECK(start_cartridge(path));
    CHECK(lab_key(SDL_KEYDOWN,SDL_SCANCODE_J));
    if (a) CHECK(bank_active(a));
    float output[2048];
    for (unsigned i=0;i<4;++i) {
        if (a) audio_output(a,(Uint8 *)output,sizeof output);
        CHECK(lab_key(SDL_KEYDOWN,SDL_SCANCODE_D));
        CHECK(lab_key(SDL_KEYUP,SDL_SCANCODE_D));
    }
    CHECK(lab_key(SDL_KEYDOWN,SDL_SCANCODE_K));
    CHECK(lab_key(SDL_KEYUP,SDL_SCANCODE_K));
    CHECK(lab_key(SDL_KEYUP,SDL_SCANCODE_J));
    if (a) {
        /* Lab release is .2 seconds; render past it without wall-clock waiting. */
        for (unsigned i=0;i<12;++i) audio_output(a,(Uint8 *)output,sizeof output);
        CHECK(!bank_active(a));
    }
    end_cartridge();
    CHECK(g_engine.audio==a);
    CHECK(start_cartridge(path));
    if (a) CHECK(!bank_active(a));
    shutdown_engine();
    puts("Synth Lab cartridge checks passed");
    return 0;
}
int main(int argc, char **argv) {
    if (argc>1) return lab(argv[1],argc>2);
    sb_audio_t *a=sb_audio_create(); CHECK(a);
    SDL_PauseAudioDevice(a->device,1);
    CHECK(fresh(a));
    CHECK(run("s=sugarbee.synth; assert(type(s)=='table'); assert(s.configure(1,{}))"));
    CHECK(!bank_active(a));
    CHECK(run("local t=setmetatable({}, {__index=function() error('must not run') end}); assert(s.configure(1,t)); local c={volume=.5}; assert(s.configure(1,c)); c.volume=0"));
    CHECK(a->bank.voices[0].volume==.5);
    CHECK(run("assert(s.configure(1,{waveform='pulse',attack=.03,volume=.8,pan=-1,filter={cutoff=700,bypass=false}}))"));
    CHECK(run("assert(s.configure(1,{release=.5,filter={cutoff=900}}))"));
    CHECK(a->bank.voices[0].settings.attack==.03 && a->bank.voices[0].settings.release==.5);
    CHECK(a->bank.voices[0].volume==.8 && a->bank.filters[0].cutoff==900);
    CHECK(a->bank.filters[0].wet==1 && a->bank.kind[0]==SB_AUDIO_VOICE_PULSE);
    sb_audio_bank_t before=a->bank;
    CHECK(run(
        "local function bad(f) assert(not pcall(f)) end "
        "for _,v in ipairs({0,10,1.5,'1',false,{}}) do bad(function() s.configure(v,{}) end) end "
        "for _,v in ipairs({'0.5',false,{},math.huge,0/0,-1,2}) do "
        "bad(function() s.configure(1,{volume=v}) end) end "
        "bad(function() s.configure(1,{volume=.1,release=11}) end) "
        "bad(function() s.configure(1,{filter={cutoff=10}}) end) "
        "bad(function() s.configure(1,{filter={bypass=1}}) end) "
        "bad(function() s.configure(1,{filter={resonance=.5}}) end) "
        "bad(function() s.configure(1,{waveform='sine'}) end) "
        "bad(function() s.configure(1,{waveform='noise',oops=1}) end) "
        "bad(function() s.configure(1,{['volume'..string.char(0)]=.1}) end) "
        "bad(function() s.configure(1,{[1]=.1}) end) "
        "bad(function() s.configure(1,{pan=-2}) end) "
        "bad(function() s.configure(1,{pulse_width=0}) end) "
        "bad(function() s.configure(1,{},true) end) "
        "bad(function() s.note_on(1) end) "
        "bad(function() s.note_on(1,'440') end) "
        "bad(function() s.note_on(1,881) end) "
        "bad(function() s.note_off(1,2) end) "
        "bad(function() s.stop() end)"));
    CHECK(!memcmp(&before,&a->bank,sizeof before));
    CHECK(run("assert(s.note_on(1,440)); assert(s.note_on(2,110)); assert(s.note_on(9,880))"));
    float out[2048];
    audio_output(a,(Uint8 *)out,sizeof out);
    CHECK(bank_active(a));
    CHECK(run("assert(s.configure(1,{waveform='saw'}))"));
    before=a->bank;
    CHECK(run("local ok,why=s.configure(1,{waveform='noise',volume=.1}); assert(ok==false and why=='audio_rejected')"));
    CHECK(!memcmp(&before,&a->bank,sizeof before));
    CHECK(run("assert(s.note_on(1,330)); assert(not pcall(s.note_on,1))")); /* Cache unchanged. */
    audio_output(a,(Uint8 *)out,sizeof out);
    CHECK(run("assert(s.configure(1,{waveform='noise'})); assert(s.note_on(1)); assert(not pcall(s.note_on,1,440))"));
    set_cartridge_paused(true);
    audio_output(a,(Uint8 *)out,sizeof out);
    before=a->bank;
    audio_output(a,(Uint8 *)out,sizeof out);
    CHECK(!memcmp(&before,&a->bank,sizeof before));
    for (unsigned i=0;i<2048;++i) CHECK(out[i]==0);
    set_cartridge_paused(false);
    CHECK(run("assert(s.note_off(1)); assert(s.stop(2)); assert(s.stop(9)); assert(s.stop(9))"));
    CHECK(run("function shutdown() assert(s.configure(1,{waveform='triangle'})); assert(s.note_on(1,220)); shutdown_ok=true end"));
    audio_output(a,(Uint8 *)out,sizeof out); /* Finish transition before shutdown. */
    call_lua_shutdown();
    CHECK(run("assert(shutdown_ok)"));
    end_cartridge(); CHECK(!bank_active(a));
    CHECK(fresh(a));
    CHECK(run("assert(sugarbee.synth.note_on(1,440)); function update() error('expected synth failure') end"));
    call_lua_global_function("update",true,.016);
    CHECK(g_engine.guru_active);
    CHECK(run("local ok,why=sugarbee.synth.note_on(1,440); assert(not ok and why=='audio_rejected')"));
    end_cartridge();
    CHECK(fresh(NULL));
    CHECK(run("s=sugarbee.synth; assert(s.configure(1,{waveform='noise'})); local ok,why=s.note_on(1); assert(not ok and why=='audio_unavailable'); assert(not pcall(s.note_on,1,440)); assert(not pcall(s.configure,1,{volume=2}))"));
    end_cartridge();
    sb_audio_destroy(a);
    puts("Lua synth checks passed");
    return 0;
}
