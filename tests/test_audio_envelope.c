#include "../src/sb_audio_envelope.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "ADSR line %d: %s\n", __LINE__, #e); return 1; } } while (0)
#define NEAR(a,b) (fabs((a)-(b)) < 1e-12)
#define FRAME (1.0 / SB_ENVELOPE_SAMPLE_RATE)

int main(void) {
    sb_envelope_t e = {0};
    sb_envelope_settings_t s = {4*FRAME, 4*FRAME, 0.5, 4*FRAME};
    CHECK(sb_envelope_advance(&e) == 0);
    sb_envelope_note_off(&e); CHECK(e.stage == SB_ENVELOPE_IDLE);
    CHECK(sb_envelope_start(&e, &s));
    CHECK(e.level == 0 && e.stage == SB_ENVELOPE_ATTACK);
    for (unsigned i = 1; i <= 4; ++i) CHECK(NEAR(sb_envelope_advance(&e), i*0.25));
    CHECK(e.stage == SB_ENVELOPE_DECAY);
    for (unsigned i = 1; i <= 4; ++i) CHECK(NEAR(sb_envelope_advance(&e), 1-i*0.125));
    CHECK(e.stage == SB_ENVELOPE_SUSTAIN);
    for (unsigned i = 0; i < 100000; ++i) CHECK(sb_envelope_advance(&e) == 0.5);
    /* Caller edits cannot change sustain or the captured future release. */
    s.sustain = 0.9; s.release = 0;
    CHECK(sb_envelope_advance(&e) == 0.5);
    unsigned char before[sizeof(e)]; memcpy(before, &e, sizeof(e));
    CHECK(!sb_envelope_start(&e, &s) && memcmp(before, &e, sizeof(e)) == 0);
    sb_envelope_note_off(&e);
    for (unsigned i = 1; i <= 4; ++i) {
        sb_envelope_note_off(&e); /* Must not restart release. */
        CHECK(NEAR(sb_envelope_advance(&e), 0.5*(1-i/4.0)));
    }
    CHECK(e.stage == SB_ENVELOPE_IDLE && e.level == 0);
    CHECK(sb_envelope_advance(&e) == 0);
    /* The caller's edited settings apply when the next idle note starts. */
    CHECK(sb_envelope_start(&e, &s));
    for (unsigned i = 0; i < 8; ++i) sb_envelope_advance(&e);
    CHECK(e.level == 0.9 && e.release_frames == 0);
    sb_envelope_note_off(&e); CHECK(e.stage == SB_ENVELOPE_IDLE);
    s = (sb_envelope_settings_t){4*FRAME, 4*FRAME, 0.5, 4*FRAME};
    /* Early note-off at attack start, mid-attack, peak, and mid-decay. */
    const unsigned steps[] = {0, 2, 4, 6};
    const double levels[] = {0, 0.5, 1, 0.75};
    for (unsigned c = 0; c < 4; ++c) {
        CHECK(sb_envelope_start(&e, &s));
        for (unsigned i = 0; i < steps[c]; ++i) sb_envelope_advance(&e);
        CHECK(NEAR(e.level, levels[c]));
        sb_envelope_note_off(&e);
        for (unsigned i = 1; i <= 4; ++i)
            CHECK(NEAR(sb_envelope_advance(&e), levels[c]*(1-i/4.0)));
        CHECK(e.stage == SB_ENVELOPE_IDLE);
    }
    /* All combinations of immediate attack, decay, release and sustain ends. */
    for (unsigned mask = 0; mask < 16; ++mask) {
        s = (sb_envelope_settings_t){(mask&1) ? 0 : FRAME,
            (mask&2) ? 0 : FRAME, (mask&8) ? 1 : 0, (mask&4) ? 0 : FRAME};
        CHECK(sb_envelope_start(&e, &s));
        for (unsigned i = 0; i < 2; ++i) sb_envelope_advance(&e);
        CHECK(e.stage == SB_ENVELOPE_SUSTAIN && e.level == s.sustain);
        sb_envelope_note_off(&e);
        if (s.release == 0) CHECK(e.stage == SB_ENVELOPE_IDLE && e.level == 0);
        sb_envelope_advance(&e);
        CHECK(e.stage == SB_ENVELOPE_IDLE && e.level == 0);
    }
    /* Explicit zero-stage boundary semantics. */
    s = (sb_envelope_settings_t){0, 0, 0.3, 0};
    CHECK(sb_envelope_start(&e, &s) && e.level == 0.3 && e.stage == SB_ENVELOPE_SUSTAIN);
    sb_envelope_note_off(&e); CHECK(e.level == 0 && e.stage == SB_ENVELOPE_IDLE);
    s = (sb_envelope_settings_t){FRAME, 0, 0.3, 0};
    CHECK(sb_envelope_start(&e, &s));
    CHECK(sb_envelope_advance(&e) == 0.3 && e.stage == SB_ENVELOPE_SUSTAIN);
    sb_envelope_note_off(&e);
    /* Nearest-frame rounding and positive subframe minimum. */
    s = (sb_envelope_settings_t){0.1*FRAME, 1.5*FRAME, 0.7, 2.49*FRAME};
    CHECK(sb_envelope_start(&e, &s));
    CHECK(e.attack_frames == 1 && e.decay_frames == 2 && e.release_frames == 2);
    sb_envelope_reset(&e);
    /* Whole request rejected for each invalid field; no partial mutation. */
    const double invalid[] = {-0.1, 11, NAN, INFINITY, -INFINITY};
    for (unsigned field = 0; field < 4; ++field) {
        for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
            s = (sb_envelope_settings_t){0.01,0.1,0.7,0.15};
            double *fields[] = {&s.attack, &s.decay, &s.sustain, &s.release};
            *fields[field] = invalid[i];
            memcpy(before, &e, sizeof(e));
            CHECK(!sb_envelope_start(&e, &s));
            CHECK(memcmp(before, &e, sizeof(e)) == 0);
        }
    }
    s = (sb_envelope_settings_t){0,0,1.00001,0};
    CHECK(!sb_envelope_start(&e, &s));
    CHECK(!sb_envelope_start(NULL, &s) && !sb_envelope_start(&e, NULL));
    sb_envelope_reset(NULL); sb_envelope_note_off(NULL); CHECK(sb_envelope_advance(NULL) == 0);
    /* Maximum durations: bounded, monotonic ramps ending at exact targets. */
    s = (sb_envelope_settings_t){10,10,0.25,10};
    CHECK(sb_envelope_start(&e, &s));
    CHECK(e.attack_frames == 480000 && e.decay_frames == 480000 && e.release_frames == 480000);
    for (unsigned stage = 0; stage < 3; ++stage) {
        if (stage == 2) sb_envelope_note_off(&e);
        double previous = e.level;
        for (unsigned i = 0; i < 480000; ++i) {
            double level = sb_envelope_advance(&e);
            CHECK(isfinite(level) && level >= 0 && level <= 1);
            CHECK(stage == 0 ? level >= previous : level <= previous);
            previous = level;
        }
        CHECK(e.level == (stage == 0 ? 1 : stage == 1 ? 0.25 : 0));
    }
    CHECK(e.stage == SB_ENVELOPE_IDLE);
    /* Retrigger from attack, decay, sustain, and release at known levels. */
    for (unsigned stage = 0; stage < 4; ++stage) {
        sb_envelope_reset(&e);
        s = (sb_envelope_settings_t){4*FRAME, 4*FRAME, 0.5, 4*FRAME};
        CHECK(sb_envelope_start(&e, &s));
        unsigned steps = stage == 0 ? 2 : stage == 1 ? 6 : 8;
        for (unsigned i = 0; i < steps; ++i) sb_envelope_advance(&e);
        if (stage == 3) { sb_envelope_note_off(&e); sb_envelope_advance(&e); }
        double level = e.level;
        sb_envelope_settings_t new_note = {8*FRAME, 4*FRAME, 0.2, 2*FRAME};
        CHECK(sb_envelope_retrigger(&e, &new_note));
        CHECK(e.level == level && e.elapsed == 0 && e.attack_frames == 8);
        for (unsigned i = 1; i <= 8; ++i)
            CHECK(NEAR(sb_envelope_advance(&e), level + (1-level)*i/8));
        CHECK(e.stage == SB_ENVELOPE_DECAY && e.release_frames == 2);
    }
    memcpy(before, &e, sizeof(e));
    s.attack = NAN;
    CHECK(!sb_envelope_retrigger(&e, &s) && memcmp(before, &e, sizeof(e)) == 0);
    puts("Standalone ADSR checks passed");
    return 0;
}
