#include "../src/sb_audio_bank.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Noise bank line %d: %s\n", __LINE__, #e); return 1; } } while (0)
static float out[8192], expected[8192], chunked[8192], raw[4096];
int main(void) {
    sb_audio_bank_t bank = {0}, copy = {0};
    sb_envelope_settings_t flat = {0,0,1,0};
    uint32_t defaults[9];
    for (unsigned v = 1; v <= 9; ++v) {
        CHECK(sb_audio_bank_noise_on(&bank, v));
        defaults[v-1] = bank.noise[v-1].state;
        CHECK(defaults[v-1] != 0);
        for (unsigned j = 0; j < v-1; ++j) CHECK(defaults[j] != defaults[v-1]);
    }
    sb_audio_bank_reset(&bank);
    /* Idle starts replay the seed exactly, including after release or stop. */
    CHECK(sb_audio_bank_configure(&bank, 1, &flat));
    CHECK(sb_audio_bank_volume(&bank, 1, 1));
    CHECK(sb_audio_bank_pan(&bank, 1, -1));
    CHECK(sb_audio_bank_seed(&bank, 1, 123456789));
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    sb_audio_noise_t source;
    CHECK(sb_audio_noise_reset(&source, 123456789));
    CHECK(sb_audio_noise_render(&source, raw, 4096));
    CHECK(sb_audio_bank_render(&bank, out, 4096));
    for (unsigned i = 0; i < 4096; ++i) {
        CHECK(out[2*i] == (float)(raw[i]*SB_AUDIO_SYNTH_GAIN));
        CHECK(out[2*i+1] == 0);
    }
    CHECK(bank.noise[0].state == source.state);
    CHECK(sb_audio_bank_note_off(&bank, 1));
    uint32_t stopped = bank.noise[0].state;
    CHECK(sb_audio_bank_render(&bank, expected, 100));
    CHECK(bank.noise[0].state == stopped);
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, expected, 4096));
    CHECK(memcmp(out, expected, sizeof(out)) == 0);
    CHECK(sb_audio_bank_stop(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, expected, 480));
    CHECK(!sb_audio_bank_active(&bank, 1));
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    CHECK(bank.noise[0].state == 123456789);
    /* No mutation for invalid requests or active cross-type changes. */
    unsigned char snapshot[sizeof(bank)];
    memcpy(snapshot, &bank, sizeof(bank));
    CHECK(!sb_audio_bank_note_on(&bank, 1, 440));
    CHECK(!sb_audio_bank_pitch(&bank, 1, 440));
    CHECK(!sb_audio_bank_noise_on(&bank, 0));
    CHECK(!sb_audio_bank_noise_on(&bank, 10));
    CHECK(!sb_audio_bank_seed(&bank, 0, 1));
    CHECK(!sb_audio_bank_seed(NULL, 1, 1));
    CHECK(!sb_audio_bank_noise_on(NULL, 1));
    CHECK(memcmp(snapshot, &bank, sizeof(bank)) == 0);
    CHECK(sb_audio_bank_note_off(&bank, 1));
    CHECK(sb_audio_bank_note_on(&bank, 1, 440));
    memcpy(snapshot, &bank, sizeof(bank));
    CHECK(!sb_audio_bank_noise_on(&bank, 1));
    CHECK(memcmp(snapshot, &bank, sizeof(bank)) == 0);
    CHECK(sb_audio_bank_note_off(&bank, 1));
    /* Full new attack from current level, preserving sequence on retrigger
     * during attack, release, and the short stop. Pending seed stays pending. */
    sb_envelope_settings_t adsr = {.02,0,.7,.02};
    CHECK(sb_audio_bank_configure(&bank, 1, &adsr));
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, out, 240));
    for (unsigned mode = 0; mode < 3; ++mode) {
        if (mode == 1) CHECK(sb_audio_bank_note_off(&bank, 1));
        if (mode == 2) CHECK(sb_audio_bank_stop(&bank, 1));
        CHECK(sb_audio_bank_render(&bank, out, 120));
        double level = bank.voices[0].envelope.level;
        uint32_t current = bank.noise[0].state;
        CHECK(sb_audio_bank_seed(&bank, 1, 987654321));
        CHECK(sb_audio_bank_noise_on(&bank, 1));
        CHECK(bank.noise[0].state == current);
        CHECK(bank.voices[0].envelope.level == level);
        CHECK(!bank.voices[0].stopping);
        CHECK(sb_audio_bank_render(&bank, out, 480));
        CHECK(fabs(bank.voices[0].envelope.level-(level+(1-level)*.5)) < 1e-12);
    }
    CHECK(sb_audio_bank_note_off(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, out, 960));
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    CHECK(bank.noise[0].state == 987654321);
    CHECK(sb_audio_bank_seed(&bank, 1, 0));
    CHECK(sb_audio_bank_stop(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, out, 480));
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    CHECK(bank.noise[0].state == defaults[0]);
    /* Muted voices still advance, including all release and stop samples. */
    sb_audio_bank_reset(&bank);
    CHECK(sb_audio_bank_configure(&bank, 1, &adsr));
    CHECK(sb_audio_bank_volume(&bank, 1, 0));
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    source = bank.noise[0];
    CHECK(sb_audio_bank_render(&bank, out, 1024));
    CHECK(sb_audio_noise_render(&source, raw, 1024));
    CHECK(bank.noise[0].state == source.state);
    for (unsigned i = 0; i < 2048; ++i) CHECK(out[i] == 0);
    CHECK(sb_audio_bank_note_off(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, out, 1024));
    CHECK(sb_audio_noise_render(&source, raw, 960));
    CHECK(bank.noise[0].state == source.state && !bank.voices[0].active);
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    source = bank.noise[0];
    CHECK(sb_audio_bank_stop(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, out, 1024));
    CHECK(sb_audio_noise_render(&source, raw, 480));
    CHECK(bank.noise[0].state == source.state && !bank.voices[0].active);
    /* Worst case: nine identical full-level noise voices; preserve 1/9 bound. */
    sb_audio_bank_reset(&bank);
    for (unsigned v = 1; v <= 9; ++v) {
        CHECK(sb_audio_bank_configure(&bank, v, &flat));
        CHECK(sb_audio_bank_volume(&bank, v, 1));
        CHECK(sb_audio_bank_pan(&bank, v, -1));
        CHECK(sb_audio_bank_seed(&bank, v, 123456789));
        CHECK(sb_audio_bank_noise_on(&bank, v));
    }
    CHECK(sb_audio_bank_render(&bank, out, 4096));
    CHECK(sb_audio_noise_reset(&source, 123456789));
    CHECK(sb_audio_noise_render(&source, raw, 4096));
    for (unsigned i = 0; i < 4096; ++i) {
        CHECK(isfinite(out[2*i]) && fabs(out[2*i]) <= 1);
        CHECK(fabs(out[2*i]-raw[i]) < 1e-7 && out[2*i+1] == 0);
    }
    /* Mixed triangle/noise render is independent of buffer partitioning. */
    sb_audio_bank_reset(&bank);
    for (unsigned v = 1; v <= 9; ++v) {
        if (v%2) CHECK(sb_audio_bank_noise_on(&bank, v));
        else CHECK(sb_audio_bank_note_on(&bank, v, 220+v*40));
        CHECK(sb_audio_bank_pan(&bank, v, (v-5.0)/4));
    }
    copy = bank;
    CHECK(sb_audio_bank_render(&bank, out, 4096));
    for (unsigned i = 0; i < 4096; i += 37)
        CHECK(sb_audio_bank_render(&copy, chunked+2*i, 4096-i < 37 ? 4096-i : 37));
    CHECK(memcmp(out, chunked, sizeof(out)) == 0);
    for (unsigned v = 0; v < 9; ++v) CHECK(copy.noise[v].state == bank.noise[v].state);
    /* Touch one slot; all other noise streams and voice states stay intact. */
    copy = bank;
    CHECK(sb_audio_bank_seed(&bank, 1, 42));
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    for (unsigned v = 1; v < 9; ++v) {
        CHECK(bank.noise[v].state == copy.noise[v].state);
        CHECK(memcmp(&bank.voices[v], &copy.voices[v], sizeof(bank.voices[v])) == 0);
    }
    sb_audio_bank_reset(&bank);
    for (unsigned v = 1; v <= 9; ++v) {
        CHECK(!bank.voices[v-1].active && !bank.noise_voice[v-1]);
        CHECK(sb_audio_bank_noise_on(&bank, v));
        CHECK(bank.noise[v-1].state == defaults[v-1]);
    }
    puts("Noise bank checks passed");
    return 0;
}
