#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#include "../src/sb_audio_sdl.c"
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Music Lab line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static void render(unsigned frames) {
    float output[2048];
    while (frames) {
        unsigned n=frames>1024?1024:frames;
        audio_output(g_engine.audio,(Uint8 *)output,(int)(n*2*sizeof(float))); frames-=n;
    }
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
static bool press(SDL_Scancode code) {
    return keypress(SDL_KEYDOWN,code) && keypress(SDL_KEYUP,code);
}
int main(int argc,char **argv) {
    CHECK(argc==3 || argc==4);
    unsigned end_frame=192000;
    if (argc==4) {
        char *end=NULL; unsigned long value=strtoul(argv[3],&end,10);
        CHECK(end!=argv[3] && *end=='\0' && value>0 && value<=28800000);
        end_frame=(unsigned)value;
    }
    bool unavailable=!strcmp(argv[2],"unavailable");
    if (unavailable) CHECK(SDL_setenv("SDL_AUDIODRIVER","sugarbee-no-device",1)==0);
    CHECK(init_engine()); CHECK((g_engine.audio==NULL)==unavailable);
    if (g_engine.audio) SDL_PauseAudioDevice(g_engine.audio->device,1);
    CHECK(start_cartridge(argv[1])); CHECK(!g_engine.guru_active);
    CHECK(press(SDL_SCANCODE_J));
    if (!unavailable) {
        CHECK(g_engine.audio->music.sequence.song->end_frame==end_frame);
        render(end_frame-1);
        CHECK(sb_audio_music_state(g_engine.audio)==SB_MUSIC_PLAYING);
        CHECK(g_engine.audio->music.sequence.frame==end_frame-1);
        render(1); CHECK(sb_audio_music_state(g_engine.audio)==SB_MUSIC_IDLE);
    }
    CHECK(press(SDL_SCANCODE_W)); CHECK(press(SDL_SCANCODE_J));
    CHECK(press(SDL_SCANCODE_I));
    if (!unavailable) {
        render(end_frame-1); CHECK(g_engine.audio->music.sequence.frame==end_frame-1);
        render(1); CHECK(g_engine.audio->music.sequence.frame==0);
        render(end_frame); CHECK(g_engine.audio->music.sequence.frame==0);
        render(100); CHECK(g_engine.audio->music.sequence.frame==100);
        CHECK(sb_audio_music_state(g_engine.audio)==SB_MUSIC_PLAYING);
        CHECK(sb_music_shared_voice_active(&g_engine.audio->music,9));
    }
    CHECK(press(SDL_SCANCODE_U)); CHECK(press(SDL_SCANCODE_U));
    if (!unavailable) {
        render(480); CHECK(g_engine.audio->music.sequence.gain==0);
    }
    set_cartridge_paused(true);
    if (!unavailable) {
        render(480); sb_music_sequence_t frozen=g_engine.audio->music.sequence;
        render(1024); CHECK(!memcmp(&frozen,&g_engine.audio->music.sequence,sizeof frozen));
    }
    set_cartridge_paused(false);
    CHECK(press(SDL_SCANCODE_U)); CHECK(press(SDL_SCANCODE_K));
    if (!unavailable) {
        render(480); CHECK(sb_audio_music_state(g_engine.audio)==SB_MUSIC_IDLE);
        CHECK(sb_music_shared_voice_active(&g_engine.audio->music,9));
    }
    CHECK(press(SDL_SCANCODE_J)); set_cartridge_paused(true);
    end_cartridge(); CHECK(!g_engine.L && !g_engine.lua_music);
    if (!unavailable) CHECK(!g_engine.audio->music_assets.bytes);
    CHECK(start_cartridge(argv[1])); CHECK(press(SDL_SCANCODE_J));
    if (!unavailable) {
        CHECK(g_engine.audio->music.sequence.gain==.8);
        CHECK(!g_engine.audio->music.sequence.loop);
        CHECK(!sb_music_shared_voice_active(&g_engine.audio->music,9));
    }
    end_cartridge(); shutdown_engine();
    puts("Music Lab cartridge checks passed"); return 0;
}
