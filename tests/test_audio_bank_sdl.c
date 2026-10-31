/* Exercise the real backend controls and callback with a paused dummy device.
 * Including the backend lets us compare its output to the pure bank contract. */
#include "../src/sb_audio_sdl.c"
#include <math.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Bank SDL line %d: %s\n", __LINE__, #e); return 1; } } while (0)
static float output[2048], expected[2048];
int main(void) {
    CHECK(!sb_audio_voice_note_on(NULL, 1, 440));
    CHECK(!sb_audio_voice_noise_on(NULL, 1));
    CHECK(!sb_audio_voice_seed(NULL, 1, 42));
    CHECK(!sb_audio_voice_configure(NULL, 1, 0, 0, 1, 0));
    CHECK(!sb_audio_voice_pitch(NULL, 1, 440));
    CHECK(!sb_audio_voice_volume(NULL, 1, 1));
    CHECK(!sb_audio_voice_pan(NULL, 1, 0));
    CHECK(!sb_audio_voice_note_off(NULL, 1));
    CHECK(!sb_audio_voice_stop(NULL, 1));
    CHECK(!sb_audio_voice_active(NULL, 1));
    sb_audio_t *audio = sb_audio_create();
    CHECK(audio);
    SDL_PauseAudioDevice(audio->device, 1);
    audio_output(audio, (Uint8 *)output, sizeof(output));
    for (unsigned i = 0; i < 2048; ++i) CHECK(output[i] == 0);
    CHECK(sb_audio_start_test_tone(audio));
    CHECK(!sb_audio_voice_note_on(audio, 1, 440));
    CHECK(!sb_audio_voice_noise_on(audio, 1));
    audio->tone.active = false;
    CHECK(sb_audio_start_triangle(audio, 440));
    CHECK(!sb_audio_voice_note_on(audio, 9, 440));
    CHECK(!sb_audio_voice_noise_on(audio, 9));
    audio->triangle = (sb_audio_triangle_t){0};
    const unsigned counts[] = {1, 3, 9};
    for (unsigned group = 0; group < 3; ++group) {
        sb_audio_bank_reset(&audio->bank);
        unsigned count = counts[group];
        for (unsigned v = 1; v <= count; ++v) {
            CHECK(sb_audio_voice_configure(audio, v, 0, 0, 1, .01));
            CHECK(sb_audio_voice_volume(audio, v, 1));
            CHECK(sb_audio_voice_pan(audio, v, -1));
            CHECK(sb_audio_voice_note_on(audio, v, 440));
            CHECK(sb_audio_voice_active(audio, v));
        }
        sb_audio_bank_t reference = audio->bank;
        CHECK(sb_audio_bank_render(&reference, expected, 1024));
        audio_output(audio, (Uint8 *)output, sizeof(output));
        float peak = 0;
        for (unsigned i = 0; i < 2048; ++i) {
            CHECK(isfinite(output[i]));
            CHECK(fabsf(output[i] - expected[i]*0.790569415f) < 1e-8f);
            CHECK(fabsf(output[i]) <= .790570f);
            if (fabsf(output[i]) > peak) peak = fabsf(output[i]);
        }
        CHECK(peak > .78f*count/9); /* Fixed headroom, no active-count scaling. */
        CHECK(!sb_audio_start_test_tone(audio));
        CHECK(!sb_audio_start_triangle(audio, 440));
    }
    unsigned char snapshot[sizeof(audio->bank)];
    memcpy(snapshot, &audio->bank, sizeof(snapshot));
    CHECK(!sb_audio_voice_note_on(audio, 0, 440));
    CHECK(!sb_audio_voice_note_on(audio, 10, 440));
    CHECK(!sb_audio_voice_configure(audio, 1, -1, 0, 1, 0));
    CHECK(!sb_audio_voice_pitch(audio, 1, NAN));
    CHECK(!sb_audio_voice_volume(audio, 1, 2));
    CHECK(!sb_audio_voice_pan(audio, 9, INFINITY));
    CHECK(!sb_audio_voice_note_off(audio, 10));
    CHECK(!sb_audio_voice_stop(audio, 0));
    CHECK(memcmp(snapshot, &audio->bank, sizeof(snapshot)) == 0);
    CHECK(sb_audio_voice_pitch(audio, 1, 660));
    CHECK(sb_audio_voice_volume(audio, 1, .5));
    CHECK(sb_audio_voice_pan(audio, 1, 1));
    CHECK(sb_audio_voice_note_on(audio, 1, 330));
    CHECK(sb_audio_voice_note_off(audio, 1));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!sb_audio_voice_active(audio, 1));
    for (unsigned v = 2; v <= 9; ++v) {
        CHECK(sb_audio_voice_active(audio, v));
        CHECK(sb_audio_voice_stop(audio, v));
    }
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!bank_active(audio));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    for (unsigned i = 0; i < 2048; ++i) CHECK(output[i] == 0);
    CHECK(sb_audio_start_test_tone(audio));
    audio->tone.active = false;
    sb_audio_bank_reset(&audio->bank);
    CHECK(sb_audio_voice_seed(audio, 1, 123456789));
    CHECK(sb_audio_voice_configure(audio, 1, 0, 0, 1, .01));
    CHECK(sb_audio_voice_noise_on(audio, 1));
    CHECK(sb_audio_voice_note_on(audio, 2, 440));
    sb_audio_bank_t reference = audio->bank;
    CHECK(sb_audio_bank_render(&reference, expected, 1024));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    for (unsigned i = 0; i < 2048; ++i)
        CHECK(fabsf(output[i]-expected[i]*0.790569415f) < 1e-8f);
    CHECK(!sb_audio_start_test_tone(audio) && !sb_audio_start_triangle(audio, 440));
    CHECK(!sb_audio_voice_note_on(audio, 1, 440));
    CHECK(!sb_audio_voice_noise_on(audio, 2));
    CHECK(!sb_audio_voice_pitch(audio, 1, 440));
    CHECK(!sb_audio_voice_noise_on(audio, 10));
    CHECK(!sb_audio_voice_seed(audio, 0, 42));
    uint32_t current = audio->bank.noise[0].state;
    CHECK(sb_audio_voice_seed(audio, 1, 42));
    CHECK(sb_audio_voice_noise_on(audio, 1));
    CHECK(audio->bank.noise[0].state == current);
    CHECK(sb_audio_voice_note_off(audio, 1));
    audio_output(audio, (Uint8 *)output, sizeof(output));
    CHECK(!sb_audio_voice_active(audio, 1) && sb_audio_voice_active(audio, 2));
    CHECK(sb_audio_voice_noise_on(audio, 1));
    CHECK(audio->bank.noise[0].state == 42);
    sb_audio_bank_reset(&audio->bank);
    /* Destroy while nine mixed voices are active; no release wait needed. */
    for (unsigned v = 1; v <= 9; ++v) {
        if (v%2) CHECK(sb_audio_voice_noise_on(audio, v));
        else CHECK(sb_audio_voice_note_on(audio, v, 440));
    }
    SDL_PauseAudioDevice(audio->device, 0);
    sb_audio_destroy(audio);
    CHECK(SDL_WasInit(SDL_INIT_AUDIO) == 0);
    audio = sb_audio_create();
    CHECK(audio);
    SDL_PauseAudioDevice(audio->device, 1);
    CHECK(!bank_active(audio));
    sb_audio_destroy(audio);
    puts("SDL bank checks passed");
    return 0;
}
