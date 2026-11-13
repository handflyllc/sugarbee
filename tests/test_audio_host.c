/* Test real host lifetime with SDL dummy output; inject only open failure. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main

SDL_AudioDeviceID __real_SDL_OpenAudioDevice(const char *, int,
    const SDL_AudioSpec *, SDL_AudioSpec *, int);
void __real_SDL_CloseAudioDevice(SDL_AudioDeviceID device);
static bool fail_open, format_ok, silence_ok, shutdown_error;
static unsigned opens, closes, shutdown_calls, shutdown_audio_ok;
static SDL_AudioCallback original_callback;
static void *original_userdata;
static SDL_atomic_t callbacks;
static SDL_AudioDeviceID observed_device;

static void observe_callback(void *userdata, Uint8 *stream, int length) {
    (void)userdata;
    original_callback(original_userdata, stream, length);
    SDL_AtomicAdd(&callbacks, 1);
}
SDL_AudioDeviceID __wrap_SDL_OpenAudioDevice(const char *device, int capture,
    const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int changes) {
    ++opens;
    format_ok = !device && !capture && desired->freq == 48000 &&
        desired->format == AUDIO_F32SYS && desired->channels == 2 &&
        desired->samples == 1024 && desired->callback &&
        changes == SDL_AUDIO_ALLOW_SAMPLES_CHANGE;
    if (fail_open) { SDL_SetError("injected device-open failure"); return 0; }
    /* Check every requested byte and boundary guards using the actual callback. */
    unsigned char buffer[8194]; memset(buffer, 0xa5, sizeof(buffer));
    desired->callback(desired->userdata, buffer + 1, 8192);
    silence_ok = buffer[0] == 0xa5 && buffer[8193] == 0xa5;
    for (size_t i = 1; i < 8193; ++i) silence_ok = silence_ok && buffer[i] == 0;
    original_callback = desired->callback;
    original_userdata = desired->userdata;
    SDL_AudioSpec observed = *desired;
    observed.callback = observe_callback;
    observed_device = __real_SDL_OpenAudioDevice(device, capture, &observed, obtained, changes);
    return observed_device;
}
void __wrap_SDL_CloseAudioDevice(SDL_AudioDeviceID device) {
    ++closes;
    __real_SDL_CloseAudioDevice(device);
}
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Audio host check failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)
static int check_shutdown(lua_State *L) {
    (void)L;
    ++shutdown_calls;
    if (!g_engine.audio || closes != 0) return luaL_error(L, "audio closed before Lua shutdown");
    if (!sb_audio_voice_note_on(g_engine.audio, 1, 440))
        return luaL_error(L, "audio unavailable during eligible shutdown");
    ++shutdown_audio_ok;
    if (shutdown_error) return luaL_error(L, "injected shutdown error");
    return 0;
}
static int fail_update(lua_State *L) {
    return luaL_error(L, "injected runtime failure");
}
static bool callback_silent(void) {
    float samples[2048];
    original_callback(original_userdata, (Uint8 *)samples, sizeof(samples));
    for (size_t i=0; i<2048; ++i) if (samples[i]!=0) return false;
    return true;
}
static bool session(void) {
    g_engine.guru_active = false; /* Simulate returning from Guru to launcher. */
    sb_audio_session_begin(g_engine.audio);
    g_engine.L = luaL_newstate();
    if (!g_engine.L) return false;
    g_engine.cart_loaded = true;
    g_engine.cart_initialized = true;
    lua_pushcfunction(g_engine.L, check_shutdown);
    lua_setglobal(g_engine.L, "shutdown");
    return true;
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    bool available = !strcmp(argv[1], "silence");
    fail_open = !strcmp(argv[1], "open_failure");
    if (!available && !fail_open) {
        CHECK(!strcmp(argv[1], "unavailable"));
        CHECK(SDL_setenv("SDL_AUDIODRIVER", "sugarbee-nonexistent-driver", 1) == 0);
    }
    CHECK(init_engine());
    CHECK(g_engine.is_running && g_engine.graphics && g_engine.input);
    CHECK((g_engine.audio != NULL) == available);
    if (available) {
        CHECK(format_ok && silence_ok && opens == 1);
        Uint32 start = SDL_GetTicks();
        while (SDL_AtomicGet(&callbacks) == 0 && SDL_GetTicks() - start < 2000) SDL_Delay(5);
        CHECK(SDL_AtomicGet(&callbacks) > 0);
        CHECK(sb_audio_start_test_tone(g_engine.audio));
        CHECK(!sb_audio_start_test_tone(g_engine.audio)); /* Does not retrigger. */
        CHECK(!sb_audio_start_triangle(g_engine.audio, 440));
        start = SDL_GetTicks();
        while (sb_audio_test_tone_active(g_engine.audio) && SDL_GetTicks() - start < 2500)
            SDL_Delay(5);
        CHECK(!sb_audio_test_tone_active(g_engine.audio));
        CHECK(sb_audio_configure_triangle(g_engine.audio, 0.01, 0.1, 0.7, 0.15));
        CHECK(!sb_audio_configure_triangle(g_engine.audio, -1, 0.1, 0.7, 0.15));
        CHECK(sb_audio_start_triangle(g_engine.audio, 220));
        CHECK(!sb_audio_start_test_tone(g_engine.audio));
        CHECK(sb_audio_start_triangle(g_engine.audio, 440));
        CHECK(sb_audio_set_triangle_pitch(g_engine.audio, 660));
        /* Normalized synthesis must retain quiet diagnostic output at the device. */
        float samples[2048];
        SDL_LockAudioDevice(observed_device);
        original_callback(original_userdata, (Uint8 *)samples, sizeof(samples));
        SDL_UnlockAudioDevice(observed_device);
        float peak = 0;
        for (size_t i = 0; i < 2048; ++i) {
            float magnitude = samples[i] < 0 ? -samples[i] : samples[i];
            CHECK(magnitude <= 0.017679f); /* 0.05 * 0.5 * sqrt(0.5). */
            if (magnitude > peak) peak = magnitude;
        }
        CHECK(peak > 0.005f);
        CHECK(sb_audio_set_triangle_volume(g_engine.audio, 0));
        CHECK(sb_audio_set_triangle_pan(g_engine.audio, 1));
        CHECK(!sb_audio_set_triangle_volume(g_engine.audio, 2));
        CHECK(!sb_audio_set_triangle_pan(g_engine.audio, -2));
        CHECK(!sb_audio_set_triangle_pitch(g_engine.audio, 5000));
        sb_audio_note_off_triangle(g_engine.audio);
        sb_audio_stop_triangle(g_engine.audio);
        sb_audio_stop_triangle(g_engine.audio);
        start = SDL_GetTicks();
        while (sb_audio_triangle_active(g_engine.audio) && SDL_GetTicks() - start < 2000)
            SDL_Delay(5);
        CHECK(!sb_audio_triangle_active(g_engine.audio));
        sb_audio_t *audio = g_engine.audio;
        CHECK(session()); end_cartridge();
        CHECK(shutdown_calls == 1 && g_engine.audio == audio && closes == 0 && opens == 1);
        SDL_PauseAudioDevice(observed_device, 1);
        CHECK(!sb_audio_voice_note_on(audio,1,440));
        CHECK(session());
        CHECK(sb_audio_voice_note_on(audio,1,440));
        set_cartridge_paused(true);
        (void)callback_silent(); /* Render the pause fade. */
        CHECK(callback_silent());
        set_cartridge_paused(false);
        CHECK(!callback_silent());
        set_cartridge_paused(true);
        shutdown_error = true;
        end_cartridge(); /* Paused shutdown can issue commands, even if it errors. */
        shutdown_error = false;
        CHECK(shutdown_calls == 2 && callback_silent());
        CHECK(!sb_audio_voice_note_on(audio,1,440));
        CHECK(session());
        CHECK(sb_audio_voice_note_on(audio,1,440));
        lua_pushcfunction(g_engine.L, fail_update);
        lua_setglobal(g_engine.L, "update");
        call_lua_global_function("update", true, .016);
        CHECK(g_engine.guru_active && !sb_audio_voice_note_on(audio,1,440));
        (void)callback_silent();
        CHECK(callback_silent());
        end_cartridge();
        CHECK(shutdown_calls == 2); /* Failed sessions skip Lua shutdown. */
        CHECK(!start_cartridge("/nonexistent/sugarbee-audio-test.vcart"));
        CHECK(!g_engine.L && callback_silent());
        CHECK(!sb_audio_voice_note_on(audio,1,440));
        CHECK(session()); /* Recovery retains the same device. */
        CHECK(g_engine.audio == audio && opens == 1 && closes == 0);
        CHECK(sb_audio_voice_note_on(audio,1,440));
    } else {
        CHECK(opens == (fail_open ? 1u : 0u) && closes == 0);
        CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
        CHECK(!g_engine.guru_active);
    }
    shutdown_engine();
    CHECK(!g_engine.audio && SDL_WasInit(0) == 0);
    CHECK(closes == (available ? 1u : 0u));
    if (available) CHECK(shutdown_calls == 3 && shutdown_audio_ok == 3);
    int count = SDL_AtomicGet(&callbacks);
    SDL_Delay(30);
    CHECK(SDL_AtomicGet(&callbacks) == count);
    CHECK(!sb_audio_start_test_tone(NULL) && !sb_audio_test_tone_active(NULL));
    CHECK(!sb_audio_start_triangle(NULL, 440));
    CHECK(!sb_audio_set_triangle_pitch(NULL, 440));
    CHECK(!sb_audio_triangle_active(NULL));
    sb_audio_stop_triangle(NULL);
    CHECK(!sb_audio_set_triangle_volume(NULL, .5));
    CHECK(!sb_audio_set_triangle_pan(NULL, 0));
    sb_audio_destroy(NULL);
    puts("Audio host lifecycle checks passed");
    return 0;
}
