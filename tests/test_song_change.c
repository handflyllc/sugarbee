/* Run the example's actual Lua policy through production host/audio bindings. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#include "../src/sb_audio_sdl.c"
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Song change line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static void render(unsigned frames) {
    float output[1600];
    while (frames) {
        unsigned n=frames>800?800:frames;
        audio_output(g_engine.audio,(Uint8 *)output,(int)(n*2*sizeof(float)));
        frames-=n;
    }
}
static int advance(lua_State *L) {
    (void)L; if (g_engine.audio) render(800); return 0;
}
static bool run(const char *code) {
    if (luaL_dostring(g_engine.L,code)!=LUA_OK) {
        fprintf(stderr,"%s\n",lua_tostring(g_engine.L,-1)); return false;
    }
    return true;
}
static bool press(SDL_Scancode code) {
    for (int down=1; down>=0; --down) {
        SDL_Event e={0}; e.type=e.key.type=down?SDL_KEYDOWN:SDL_KEYUP;
        e.key.keysym.scancode=code;
        if (SDL_PushEvent(&e)!=1 || !sb_input_poll(g_engine.input)) return false;
        sb_input_begin_update(g_engine.input);
        call_lua_global_function("update",true,1.0/60);
        sb_input_end_update(g_engine.input);
    }
    call_lua_global_function("draw",false,0);
    return !g_engine.guru_active;
}
int main(int argc,char **argv) {
    CHECK(argc==4);
    bool unavailable=!strcmp(argv[3],"unavailable");
    if (unavailable) CHECK(SDL_setenv("SDL_AUDIODRIVER","sugarbee-no-device",1)==0);
    CHECK(init_engine()); CHECK((g_engine.audio==NULL)==unavailable);
    if (g_engine.audio) SDL_PauseAudioDevice(g_engine.audio->device,1);
    CHECK(start_cartridge(argv[1])); CHECK(!g_engine.guru_active);
    lua_pushcfunction(g_engine.L,advance); lua_setglobal(g_engine.L,"test_advance");
    lua_pushboolean(g_engine.L,unavailable); lua_setglobal(g_engine.L,"test_unavailable");
    int result=luaL_dofile(g_engine.L,argv[2]);
    if (result) fprintf(stderr,"%s\n",lua_tostring(g_engine.L,-1));
    CHECK(result==LUA_OK);
    if (!unavailable) {
        /* Real synth contention, not the injected policy branch. */
        CHECK(run("assert(sugarbee.synth.note_on(1,330)); assert(request_song('A')); update(); "
                  "assert(song_change.error=='voice_busy' and song_change.pending==nil); "
                  "assert(sugarbee.synth.stop(1))"));
        render(800);
        CHECK(run("assert(request_song('B')); update(); assert(song_change.current=='B')"));
        render(800);
        CHECK(g_engine.audio->music.sequence.bank.retained_pitch[0]==660);
        CHECK(run("assert(request_song('A')); update()"));
        render(479); CHECK(sb_audio_music_state(g_engine.audio)==SB_MUSIC_FINISHING);
        render(1); CHECK(sb_audio_music_state(g_engine.audio)==SB_MUSIC_IDLE);
        CHECK(run("update(); assert(song_change.current=='A')"));
        render(1); CHECK(g_engine.audio->music.sequence.bank.retained_pitch[0]==440);
    }
    end_cartridge(); CHECK(!g_engine.L);
    if (g_engine.audio) CHECK(!g_engine.audio->music_assets.bytes);
    CHECK(start_cartridge(argv[1])); CHECK(!g_engine.guru_active);
    CHECK(run("assert(song_change.current==nil and song_change.pending==nil)"));
    CHECK(press(SDL_SCANCODE_J));
    if (!unavailable) {
        CHECK(run("assert(song_change.current=='A')")); render(800);
        CHECK(press(SDL_SCANCODE_K));
        CHECK(run("assert(song_change.pending=='B' and song_change.current==nil)"));
        render(480); CHECK(run("update(); assert(song_change.current=='B')"));
        render(800); CHECK(press(SDL_SCANCODE_U)); render(480);
        CHECK(run("update(); assert(song_change.current==nil and song_change.pending==nil)"));
    } else CHECK(run("assert(song_change.error=='audio_unavailable')"));
    end_cartridge(); shutdown_engine(); return 0;
}
