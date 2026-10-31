#include "../src/sb_audio_bank.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Saw bank line %d: %s\n", __LINE__, #e); return 1; } } while (0)
static float output[8192], expected[8192], raw[4096];
int main(void) {
    sb_audio_bank_t bank = {0}, copy;
    sb_envelope_settings_t flat = {0,0,1,0};
    CHECK(!sb_audio_bank_saw_on(NULL, 1, 440));
    CHECK(!sb_audio_bank_saw_on(&bank, 0, 440));
    CHECK(!sb_audio_bank_saw_on(&bank, 10, 440));
    CHECK(!sb_audio_bank_saw_on(&bank, 1, NAN));
    CHECK(!bank.voices[0].active && bank.kind[0] == SB_AUDIO_VOICE_TRIANGLE);
    /* Each slot independently routes the normalized saw with fixed 1/9 gain. */
    for (unsigned v = 1; v <= 9; ++v) {
        sb_audio_bank_reset(&bank);
        CHECK(sb_audio_bank_configure(&bank, v, &flat));
        CHECK(sb_audio_bank_volume(&bank, v, 1));
        CHECK(sb_audio_bank_pan(&bank, v, -1));
        CHECK(sb_audio_bank_saw_on(&bank, v, 440));
        sb_audio_saw_t reference;
        CHECK(sb_audio_saw_reset(&reference, 440));
        CHECK(sb_audio_saw_render(&reference, raw, 4096));
        CHECK(sb_audio_bank_render(&bank, output, 4096));
        for (unsigned i = 0; i < 4096; ++i)
            CHECK(output[2*i] == (float)(raw[i]*SB_AUDIO_SYNTH_GAIN) && output[2*i+1] == 0);
        CHECK(bank.saw[v-1].phase == reference.phase);
        CHECK(sb_audio_bank_note_off(&bank, v));
        CHECK(sb_audio_bank_saw_on(&bank, v, 440));
        CHECK(bank.saw[v-1].phase == .5);
    }
    sb_audio_bank_reset(&bank);
    sb_envelope_settings_t adsr = {.02,0,.7,.02};
    CHECK(sb_audio_bank_configure(&bank, 1, &adsr));
    CHECK(sb_audio_bank_saw_on(&bank, 1, 220));
    CHECK(sb_audio_bank_render(&bank, output, 240));
    for (unsigned mode = 0; mode < 3; ++mode) {
        if (mode == 1) CHECK(sb_audio_bank_note_off(&bank, 1));
        if (mode == 2) CHECK(sb_audio_bank_stop(&bank, 1));
        CHECK(sb_audio_bank_render(&bank, output, 120));
        double phase = bank.saw[0].phase, level = bank.voices[0].envelope.level;
        CHECK(sb_audio_bank_saw_on(&bank, 1, 660));
        CHECK(bank.saw[0].phase == phase && bank.saw[0].frequency == 660);
        CHECK(bank.voices[0].envelope.level == level && !bank.voices[0].stopping);
        CHECK(sb_audio_bank_render(&bank, output, 480));
        CHECK(fabs(bank.voices[0].envelope.level - (level+(1-level)*.5)) < 1e-12);
    }
    double phase = bank.saw[0].phase;
    sb_envelope_t envelope = bank.voices[0].envelope;
    CHECK(sb_audio_bank_pitch(&bank, 1, 330));
    CHECK(bank.saw[0].phase == phase && bank.saw[0].frequency == 330);
    CHECK(memcmp(&envelope, &bank.voices[0].envelope, sizeof(envelope)) == 0);
    unsigned char snapshot[sizeof(bank)];
    memcpy(snapshot, &bank, sizeof(bank));
    CHECK(!sb_audio_bank_saw_on(&bank, 1, 661));
    CHECK(!sb_audio_bank_pitch(&bank, 1, INFINITY));
    CHECK(!sb_audio_bank_note_on(&bank, 1, 440));
    CHECK(!sb_audio_bank_noise_on(&bank, 1));
    CHECK(memcmp(snapshot, &bank, sizeof(bank)) == 0);
    CHECK(sb_audio_bank_stop(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, output, 480));
    CHECK(!sb_audio_bank_pitch(&bank, 1, 440));
    CHECK(sb_audio_bank_note_on(&bank, 1, 440));
    memcpy(snapshot, &bank, sizeof(bank));
    CHECK(!sb_audio_bank_saw_on(&bank, 1, 440));
    CHECK(memcmp(snapshot, &bank, sizeof(bank)) == 0);
    CHECK(sb_audio_bank_stop(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, output, 480));
    CHECK(sb_audio_bank_noise_on(&bank, 1));
    CHECK(!sb_audio_bank_saw_on(&bank, 1, 440));
    /* Silence through mute still advances exactly through release/stop. */
    sb_audio_bank_reset(&bank);
    CHECK(sb_audio_bank_configure(&bank, 1, &adsr));
    CHECK(sb_audio_bank_volume(&bank, 1, 0));
    CHECK(sb_audio_bank_saw_on(&bank, 1, 440));
    sb_audio_saw_t reference = bank.saw[0];
    CHECK(sb_audio_bank_render(&bank, output, 1024));
    CHECK(sb_audio_saw_render(&reference, raw, 1024));
    CHECK(bank.saw[0].phase == reference.phase);
    for (unsigned i = 0; i < 2048; ++i) CHECK(output[i] == 0);
    CHECK(sb_audio_bank_note_off(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, output, 1024));
    CHECK(sb_audio_saw_render(&reference, raw, 960));
    CHECK(bank.saw[0].phase == reference.phase && !bank.voices[0].active);
    CHECK(sb_audio_bank_saw_on(&bank, 1, 440));
    CHECK(bank.saw[0].phase == .5);
    reference = bank.saw[0];
    CHECK(sb_audio_bank_stop(&bank, 1));
    CHECK(sb_audio_bank_render(&bank, output, 1024));
    CHECK(sb_audio_saw_render(&reference, raw, 480));
    CHECK(bank.saw[0].phase == reference.phase && !bank.voices[0].active);
    /* Nine coherent maximum-level voices test the existing headroom bound. */
    for (int pan = -1; pan <= 1; ++pan) {
        sb_audio_bank_reset(&bank);
        for (unsigned v = 1; v <= 9; ++v) {
            CHECK(sb_audio_bank_configure(&bank, v, &flat));
            CHECK(sb_audio_bank_volume(&bank, v, 1));
            CHECK(sb_audio_bank_pan(&bank, v, pan));
            CHECK(sb_audio_bank_saw_on(&bank, v, 440));
        }
        CHECK(sb_audio_saw_reset(&reference, 440));
        CHECK(sb_audio_saw_render(&reference, raw, 4096));
        CHECK(sb_audio_bank_render(&bank, output, 4096));
        for (unsigned i = 0; i < 4096; ++i) {
            double left = pan == 1 ? 0 : pan == 0 ? sqrt(.5) : 1;
            double right = pan == -1 ? 0 : pan == 0 ? sqrt(.5) : 1;
            CHECK(fabs(output[2*i]-raw[i]*left) < 2e-7);
            CHECK(fabs(output[2*i+1]-raw[i]*right) < 2e-7);
            CHECK(isfinite(output[2*i]) && fabs(output[2*i]) <= 1);
            CHECK(isfinite(output[2*i+1]) && fabs(output[2*i+1]) <= 1);
        }
    }
    sb_audio_bank_reset(&bank);
    for (unsigned v = 1; v <= 9; ++v) {
        if (v%3 == 0) CHECK(sb_audio_bank_saw_on(&bank, v, 220+v*40));
        else if (v%3 == 1) CHECK(sb_audio_bank_noise_on(&bank, v));
        else CHECK(sb_audio_bank_note_on(&bank, v, 220+v*40));
        CHECK(sb_audio_bank_pan(&bank, v, (v-5.0)/4));
    }
    copy = bank;
    CHECK(sb_audio_bank_render(&bank, output, 4096));
    for (unsigned i = 0; i < 4096; i += 37)
        CHECK(sb_audio_bank_render(&copy, expected+2*i, 4096-i < 37 ? 4096-i : 37));
    CHECK(memcmp(output, expected, sizeof(output)) == 0);
    copy = bank;
    CHECK(sb_audio_bank_saw_on(&bank, 3, 660));
    CHECK(sb_audio_bank_note_off(&bank, 3));
    for (unsigned i = 0; i < 9; ++i) if (i != 2) {
        CHECK(memcmp(&bank.voices[i], &copy.voices[i], sizeof(bank.voices[i])) == 0);
        CHECK(bank.saw[i].phase == copy.saw[i].phase && bank.noise[i].state == copy.noise[i].state);
    }
    sb_audio_bank_reset(&bank);
    CHECK(sb_audio_bank_render(&bank, output, 4096));
    for (unsigned i = 0; i < 8192; ++i) CHECK(output[i] == 0);
    for (unsigned i = 0; i < 9; ++i)
        CHECK(!bank.voices[i].active && bank.kind[i] == SB_AUDIO_VOICE_TRIANGLE && bank.saw[i].frequency == 0);
    puts("Saw bank checks passed");
    return 0;
}
