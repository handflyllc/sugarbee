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
static int lab(const char *path, bool unavailable, bool musical, bool pong, bool pitch) {
    if (unavailable) CHECK(SDL_setenv("SDL_AUDIODRIVER","sugarbee-no-device",1)==0);
    CHECK(init_engine());
    sb_audio_t *a=g_engine.audio;
    CHECK((a==NULL)==unavailable);
    if (a) SDL_PauseAudioDevice(a->device,1);
    CHECK(start_cartridge(path));
    if (pong) {
        bool heard=false;
        float samples[1600];
        for (unsigned frame=0;frame<600;++frame) {
            call_lua_global_function("update",true,1.0/60);
            call_lua_global_function("draw",false,0);
            CHECK(!g_engine.guru_active);
            if (a) {
                heard=heard || bank_active(a);
                audio_output(a,(Uint8 *)samples,sizeof samples);
            }
        }
        if (a) CHECK(heard);
        end_cartridge();
        if (a) CHECK(!bank_active(a));
        CHECK(start_cartridge(path));
        if (a) CHECK(!bank_active(a));
        shutdown_engine();
        puts("Pong audio cartridge checks passed");
        return 0;
    }
    CHECK(lab_key(SDL_KEYDOWN,SDL_SCANCODE_J));
    if (a) CHECK(bank_active(a));
    float output[2048];
    if (pitch) {
        CHECK(lab_key(SDL_KEYUP,SDL_SCANCODE_J));
        for(unsigned wave=0;wave<3;++wave) {
            for(unsigned mode=0;mode<3;++mode) {
                for(unsigned frame=0;frame<480;++frame) {
                    call_lua_global_function("update",true,1.0/60);
                    CHECK(!g_engine.guru_active);
                    if(a) {
                        audio_output(a,(Uint8 *)output,800*2*sizeof(float));
                        for(unsigned i=0;i<1600;++i) CHECK(isfinite(output[i]) && fabs(output[i])<=1);
                    }
                }
                CHECK(lab_key(SDL_KEYDOWN,SDL_SCANCODE_U));
                CHECK(lab_key(SDL_KEYUP,SDL_SCANCODE_U));
            }
            CHECK(lab_key(SDL_KEYDOWN,SDL_SCANCODE_I));
            CHECK(lab_key(SDL_KEYUP,SDL_SCANCODE_I));
        }
        set_cartridge_paused(true);
        if(a) {
            audio_output(a,(Uint8 *)output,sizeof output);
            sb_audio_bank_t frozen=a->music.sequence.bank;
            audio_output(a,(Uint8 *)output,sizeof output);
            CHECK(!memcmp(&frozen,&a->music.sequence.bank,sizeof frozen));
        }
        set_cartridge_paused(false);
        CHECK(lab_key(SDL_KEYDOWN,SDL_SCANCODE_K));
        CHECK(lab_key(SDL_KEYUP,SDL_SCANCODE_K));
        if(a) {
            for(unsigned i=0;i<60;++i) audio_output(a,(Uint8 *)output,sizeof output);
            CHECK(!bank_active(a));
        }
        end_cartridge(); CHECK(start_cartridge(path));
        if(a) CHECK(!bank_active(a));
        shutdown_engine();
        return 0;
    }
    if (musical) {
        CHECK(lab_key(SDL_KEYUP,SDL_SCANCODE_J));
        set_cartridge_paused(true);
        if (a) {
            audio_output(a,(Uint8 *)output,sizeof output);
            sb_audio_bank_t frozen=a->music.sequence.bank;
            audio_output(a,(Uint8 *)output,sizeof output);
            CHECK(!memcmp(&frozen,&a->music.sequence.bank,sizeof frozen));
            for (unsigned i=0;i<2048;++i) CHECK(output[i]==0);
        }
        set_cartridge_paused(false);
        for (unsigned frame=0;frame<600;++frame) {
            call_lua_global_function("update",true,1.0/60);
            CHECK(!g_engine.guru_active);
            if (a) audio_output(a,(Uint8 *)output,800*2*sizeof(float));
        }
        if (a) CHECK(!bank_active(a));
        CHECK(lab_key(SDL_KEYDOWN,SDL_SCANCODE_J));
        if (a) CHECK(bank_active(a));
        CHECK(lab_key(SDL_KEYUP,SDL_SCANCODE_J));
        CHECK(lab_key(SDL_KEYDOWN,SDL_SCANCODE_K));
        if (a) {
            audio_output(a,(Uint8 *)output,sizeof output);
            CHECK(!bank_active(a));
        }
        end_cartridge();
        CHECK(start_cartridge(path));
        if (a) CHECK(!bank_active(a));
        shutdown_engine();
        puts("Musical Synth Lab cartridge checks passed");
        return 0;
    }
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
    if (argc>1) return lab(argv[1],argc>2 && strstr(argv[2],"unavailable"),
                          argc>2 && strstr(argv[2],"music"),
                          argc>2 && strstr(argv[2],"pong"),
                          argc>2 && strstr(argv[2],"pitch"));
    sb_audio_t *a=sb_audio_create(); CHECK(a);
    SDL_PauseAudioDevice(a->device,1);
    CHECK(fresh(a));
    CHECK(run("s=sugarbee.synth; assert(type(s)=='table'); assert(s.configure(1,{}))"));
    CHECK(!bank_active(a));
    CHECK(run("local r=table.pack(s.set_frequency(1,440)); assert(r.n==2 and r[1]==false and r[2]=='audio_rejected')"));
    CHECK(run("local t=setmetatable({}, {__index=function() error('must not run') end}); assert(s.configure(1,t)); local c={volume=.5}; assert(s.configure(1,c)); c.volume=0"));
    CHECK(a->music.sequence.bank.voices[0].volume==.5);
    CHECK(run("assert(s.configure(1,{waveform='pulse',attack=.03,volume=.8,pan=-1,filter={cutoff=700,bypass=false}}))"));
    CHECK(run("assert(s.configure(1,{release=.5,filter={cutoff=900}}))"));
    CHECK(a->music.sequence.bank.voices[0].settings.attack==.03 && a->music.sequence.bank.voices[0].settings.release==.5);
    CHECK(a->music.sequence.bank.voices[0].volume==.8 && a->music.sequence.bank.filters[0].cutoff==900);
    CHECK(a->music.sequence.bank.filters[0].wet==1 && a->music.sequence.bank.kind[0]==SB_AUDIO_VOICE_PULSE);
    sb_audio_bank_t before=a->music.sequence.bank;
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
        "bad(function() s.note_on(1,4186.011) end) "
        "bad(function() s.note_on(1,27.499999999999996) end) "
        "bad(function() s.note_on(1,4186.010000000001) end) "
        "bad(function() s.note_on(1,math.huge) end) "
        "bad(function() s.note_on(1,0/0) end) "
        "bad(function() s.note_off(1,2) end) "
        "bad(function() s.stop() end) "
        "bad(function() s.set_frequency() end) "
        "bad(function() s.set_frequency(1,440,nil) end) "
        "for _,v in ipairs({0,10,1.5,'1',false,{}}) do bad(function() s.set_frequency(v,440) end) end "
        "for _,v in ipairs({'440',false,{},math.huge,0/0,27.499999999999996,4186.010000000001}) do "
        "bad(function() s.set_frequency(1,v) end) end"));
    CHECK(!memcmp(&before,&a->music.sequence.bank,sizeof before));
    CHECK(run("assert(s.note_on(1,440)); assert(s.note_on(2,27.5)); assert(s.note_on(9,4186.01))"));
    CHECK(a->music.sequence.bank.retained_pitch[1]==27.5);
    CHECK(a->music.sequence.bank.retained_pitch[8]==4186.01);
    CHECK(run("assert(s.note_on(9,4186.009044809578))"));
    CHECK(a->music.sequence.bank.retained_pitch[8]==4186.009044809578);
    float out[2048];
    audio_output(a,(Uint8 *)out,sizeof out);
    CHECK(bank_active(a));
    /* Compare the entire bank after accounting for frequency fields only.
       This catches phase, envelope, filter and control-ramp restarts. */
    before=a->music.sequence.bank;
    CHECK(run("local r=table.pack(s.set_frequency(1,880.25)); assert(r.n==1 and r[1]); "
              "assert(s.set_frequency(2,27.5)); assert(s.set_frequency(9,4186.01))"));
    before.retained_pitch[0]=880.25; before.pulse[0].frequency=880.25;
    before.retained_pitch[8]=4186.01; before.voices[8].frequency=4186.01;
    CHECK(!memcmp(&before,&a->music.sequence.bank,sizeof before));
    CHECK(run("assert(s.configure(1,{waveform='saw'}))"));
    before=a->music.sequence.bank;
    CHECK(run("assert(s.set_frequency(1,660))"));
    before.retained_pitch[0]=660; before.pulse[0].frequency=660; before.saw[0].frequency=660;
    CHECK(!memcmp(&before,&a->music.sequence.bank,sizeof before));
    CHECK(run("local ok,why=s.configure(1,{waveform='noise',volume=.1}); assert(ok==false and why=='audio_rejected')"));
    CHECK(!memcmp(&before,&a->music.sequence.bank,sizeof before));
    CHECK(run("assert(s.note_on(1,330)); assert(not pcall(s.note_on,1))")); /* Cache unchanged. */
    audio_output(a,(Uint8 *)out,sizeof out);
    CHECK(run("assert(s.configure(1,{waveform='noise'})); assert(s.note_on(1)); assert(not pcall(s.note_on,1,440))"));
    before=a->music.sequence.bank;
    CHECK(run("local ok,why=s.set_frequency(1,440); assert(not ok and why=='audio_rejected')"));
    CHECK(!memcmp(&before,&a->music.sequence.bank,sizeof before));
    set_cartridge_paused(true);
    audio_output(a,(Uint8 *)out,sizeof out);
    before=a->music.sequence.bank;
    audio_output(a,(Uint8 *)out,sizeof out);
    CHECK(!memcmp(&before,&a->music.sequence.bank,sizeof before));
    for (unsigned i=0;i<2048;++i) CHECK(out[i]==0);
    set_cartridge_paused(false);
    CHECK(run("assert(s.note_off(2))"));
    audio_output(a,(Uint8 *)out,16*sizeof(float));
    before=a->music.sequence.bank;
    CHECK(run("assert(s.set_frequency(2,330.5))"));
    before.voices[1].frequency=330.5; before.retained_pitch[1]=330.5;
    CHECK(!memcmp(&before,&a->music.sequence.bank,sizeof before));
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
    CHECK(run("local ok,why=sugarbee.synth.note_on(1,440); assert(not ok and why=='audio_rejected'); "
              "ok,why=sugarbee.synth.set_frequency(1,440); assert(not ok and why=='audio_rejected')"));
    end_cartridge();
    CHECK(fresh(NULL));
    CHECK(run("s=sugarbee.synth; assert(s.configure(1,{waveform='noise'})); local ok,why=s.note_on(1); assert(not ok and why=='audio_unavailable'); assert(not pcall(s.note_on,1,440)); assert(not pcall(s.configure,1,{volume=2}))"));
    CHECK(run("local ok,why=s.set_frequency(2,440); assert(not ok and why=='audio_unavailable'); "
              "assert(not pcall(s.set_frequency,2,0/0))"));
    end_cartridge();
    sb_audio_destroy(a);
    puts("Lua synth checks passed");
    return 0;
}
