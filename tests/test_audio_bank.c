#include "../src/sb_audio_bank.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Bank line %d: %s\n", __LINE__, #e); return 1; } } while (0)
#define NEAR(a,b) (fabs((double)(a)-(b)) < 2e-7)
static float output[96000], other[96000], part[96000];
static const sb_envelope_settings_t flat = {0,0,1,0};
static bool prepare(sb_audio_bank_t *bank, unsigned voice, double pitch, double pan) {
    return sb_audio_bank_configure(bank, voice, &flat) &&
           sb_audio_bank_volume(bank, voice, 1) && sb_audio_bank_pan(bank, voice, pan) &&
           sb_audio_bank_note_on(bank, voice, pitch);
}
int main(void) {
    sb_audio_bank_t bank = {0}, copy, solo = {0};
    memset(output, 0xff, sizeof(output));
    CHECK(sb_audio_bank_render(&bank, output, 48000));
    for (unsigned i = 0; i < 96000; ++i) CHECK(output[i] == 0);
    CHECK(sb_audio_bank_render(&bank, NULL, 0));
    unsigned char snapshot[sizeof(bank)]; memcpy(snapshot, &bank, sizeof(bank));
    const unsigned invalid[] = {0,10,UINT_MAX};
    for (unsigned i = 0; i < 3; ++i) {
        unsigned v = invalid[i];
        CHECK(!sb_audio_bank_configure(&bank, v, &flat));
        CHECK(!sb_audio_bank_note_on(&bank, v, 440));
        CHECK(!sb_audio_bank_pitch(&bank, v, 440));
        CHECK(!sb_audio_bank_volume(&bank, v, 1));
        CHECK(!sb_audio_bank_pan(&bank, v, 0));
        CHECK(!sb_audio_bank_note_off(&bank, v));
        CHECK(!sb_audio_bank_stop(&bank, v));
        CHECK(!sb_audio_bank_active(&bank, v));
        CHECK(memcmp(snapshot, &bank, sizeof(bank)) == 0);
    }
    CHECK(!sb_audio_bank_render(&bank, NULL, 1));
    CHECK(!sb_audio_bank_render(&bank, output, SIZE_MAX));
    CHECK(memcmp(snapshot, &bank, sizeof(bank)) == 0);
    CHECK(!sb_audio_bank_render(NULL, output, 1));
    CHECK(!sb_audio_bank_configure(NULL, 1, &flat));
    CHECK(!sb_audio_bank_note_on(NULL, 1, 440));
    CHECK(!sb_audio_bank_pitch(NULL, 1, 440));
    CHECK(!sb_audio_bank_volume(NULL, 1, 1));
    CHECK(!sb_audio_bank_pan(NULL, 1, 0));
    CHECK(!sb_audio_bank_note_off(NULL, 1) && !sb_audio_bank_stop(NULL, 1));
    sb_audio_bank_reset(NULL);
    for (unsigned v = 1; v <= 9; ++v) {
        CHECK(sb_audio_bank_stop(&bank, v) && sb_audio_bank_note_off(&bank, v));
        CHECK(!sb_audio_bank_active(&bank, v));
    }
    /* Nine coherent maximum-level voices at each stereo extreme and center. */
    for (int pan = -1; pan <= 1; ++pan) {
        sb_audio_bank_reset(&bank);
        for (unsigned v = 1; v <= 9; ++v) CHECK(prepare(&bank, v, 440, pan));
        CHECK(sb_audio_bank_render(&bank, output, 48000));
        double phase = .25, peak = 0;
        for (unsigned i = 0; i < 48000; ++i) {
            double raw = 1-4*fabs(phase-.5);
            double left = pan == 1 ? 0 : pan == 0 ? sqrt(.5) : 1;
            double right = pan == -1 ? 0 : pan == 0 ? sqrt(.5) : 1;
            CHECK(NEAR(output[2*i], raw*left) && NEAR(output[2*i+1], raw*right));
            CHECK(isfinite(output[2*i]) && fabsf(output[2*i]) <= 1);
            CHECK(isfinite(output[2*i+1]) && fabsf(output[2*i+1]) <= 1);
            double value = fmax(fabs(output[2*i]), fabs(output[2*i+1]));
            if (value > peak) peak = value;
            phase += 440.0/48000; if (phase >= 1) phase -= 1;
        }
        CHECK(peak > (pan == 0 ? .70 : .99)); /* No hidden .05 attenuation. */
    }
    /* Controls address each slot independently; rejection never touches state. */
    for (unsigned v = 1; v <= 9; ++v) {
        copy = bank;
        CHECK(sb_audio_bank_configure(&bank, v, &flat));
        CHECK(sb_audio_bank_pitch(&bank, v, 220+v*40));
        CHECK(sb_audio_bank_volume(&bank, v, .2));
        CHECK(sb_audio_bank_pan(&bank, v, -.5));
        CHECK(sb_audio_bank_note_on(&bank, v, 660));
        CHECK(sb_audio_bank_stop(&bank, v));
        for (unsigned j = 0; j < 9; ++j) if (j != v-1)
            CHECK(memcmp(&bank.voices[j], &copy.voices[j], sizeof(bank.voices[j])) == 0);
        bank = copy;
    }
    memcpy(snapshot, &bank, sizeof(bank));
    sb_envelope_settings_t bad = {0,0,NAN,0};
    CHECK(!sb_audio_bank_configure(&bank, 1, &bad));
    CHECK(!sb_audio_bank_note_on(&bank, 9, INFINITY));
    CHECK(!sb_audio_bank_pitch(&bank, 1, 20000));
    CHECK(!sb_audio_bank_pan(&bank, 9, 2));
    CHECK(!sb_audio_bank_volume(&bank, 1, -.1));
    CHECK(memcmp(snapshot, &bank, sizeof(bank)) == 0);
    /* Adding voice 9 leaves voice 1's contribution and state unchanged. */
    sb_audio_bank_reset(&bank); sb_audio_bank_reset(&solo);
    CHECK(prepare(&bank, 1, 220, -1));
    CHECK(sb_audio_bank_render(&bank, output, 137));
    copy = bank;
    CHECK(prepare(&bank, 9, 660, 1));
    CHECK(prepare(&solo, 9, 660, 1));
    CHECK(sb_audio_bank_render(&bank, output, 2000));
    CHECK(sb_audio_bank_render(&copy, other, 2000));
    CHECK(sb_audio_bank_render(&solo, part, 2000));
    for (unsigned i = 0; i < 4000; ++i) CHECK(NEAR(output[i], other[i]+part[i]));
    CHECK(memcmp(&bank.voices[0], &copy.voices[0], sizeof(bank.voices[0])) == 0);
    CHECK(memcmp(&bank.voices[8], &solo.voices[8], sizeof(bank.voices[8])) == 0);
    /* Release only voice 1; stopping voice 9 retains its slot until fade ends. */
    CHECK(sb_audio_bank_note_off(&bank, 1) && !sb_audio_bank_active(&bank, 1));
    CHECK(sb_audio_bank_active(&bank, 9));
    CHECK(sb_audio_bank_stop(&bank, 9));
    CHECK(sb_audio_bank_render(&bank, output, 479) && sb_audio_bank_active(&bank, 9));
    CHECK(sb_audio_bank_render(&bank, output, 1) && !sb_audio_bank_active(&bank, 9));
    CHECK(sb_audio_bank_render(&bank, output, 50));
    for (unsigned i = 0; i < 100; ++i) CHECK(output[i] == 0);
    /* Diverse voices and overlapping ramps are buffer-partition independent. */
    for (unsigned v = 1; v <= 9; ++v) {
        CHECK(prepare(&bank, v, 220+v*40, (v-5.0)/4));
        CHECK(sb_audio_bank_volume(&bank, v, v/10.0));
        CHECK(sb_audio_bank_pan(&bank, v, (5.0-v)/4));
    }
    copy = bank;
    CHECK(sb_audio_bank_render(&bank, output, 2000));
    for (unsigned i = 0; i < 2000; i += 37) {
        unsigned n = i+37 <= 2000 ? 37 : 2000-i;
        CHECK(sb_audio_bank_render(&copy, other+2*i, n));
    }
    CHECK(memcmp(output, other, 4000*sizeof(float)) == 0);
    CHECK(memcmp(&bank, &copy, sizeof(bank)) == 0);
    sb_audio_bank_reset(&bank);
    CHECK(sb_audio_bank_render(&bank, output, 50));
    for (unsigned i = 0; i < 100; ++i) CHECK(output[i] == 0);
    for (unsigned v = 0; v < 9; ++v) {
        CHECK(!bank.voices[v].active && !bank.voices[v].configured);
        CHECK(bank.voices[v].volume == .5 && bank.voices[v].pan == 0);
    }
    CHECK(sb_audio_bank_note_on(&bank, 9, 440));
    CHECK(bank.voices[8].envelope.settings.sustain == .7);
    puts("Standalone nine-voice bank checks passed");
    return 0;
}
