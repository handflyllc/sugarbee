#include "../src/sb_audio_triangle.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Triangle line %d: %s\n", __LINE__, #e); return 1; } } while (0)
static float a[96000], b[96000];
int main(void) {
    sb_audio_triangle_t v = {0};
    CHECK(!sb_audio_triangle_pitch(&v, 440));
    CHECK(!sb_audio_triangle_start(&v, NAN));
    CHECK(!sb_audio_triangle_start(&v, INFINITY));
    CHECK(!sb_audio_triangle_start(&v, 109));
    CHECK(!sb_audio_triangle_start(&v, 881));
    sb_audio_triangle_stop(&v);
    sb_audio_triangle_render(&v, a, 10);
    for (unsigned i = 0; i < 20; ++i) CHECK(a[i] == 0);
    for (unsigned hz = 110; hz <= 880; hz += 110) {
        memset(&v, 0, sizeof(v));
        CHECK(sb_audio_triangle_start(&v, hz));
        CHECK(sb_audio_triangle_start(&v, hz));
        sb_audio_triangle_t chunked = v;
        sb_audio_triangle_render(&v, a, 48000);
        unsigned crossings = 0;
        float peak = 0;
        for (unsigned i = 0; i < 48000; ++i) {
            CHECK(isfinite(a[2*i]) && fabsf(a[2*i]) <= 1.000001f);
            CHECK(a[2*i] == a[2*i+1]);
            if (fabsf(a[2*i]) > peak) peak = fabsf(a[2*i]);
            if (i && a[2*(i-1)] <= 0 && a[2*i] > 0) ++crossings;
            if (i < 480) CHECK(fabsf(a[2*i]) <= 1.0f*i/480 + 0.000001f);
        }
        CHECK(crossings == hz && peak > 0.34f);
        size_t offset = 0;
        while (offset < 48000) {
            size_t n = offset % 113 + 1;
            if (n > 48000-offset) n = 48000-offset;
            sb_audio_triangle_render(&chunked, b+2*offset, n);
            offset += n;
        }
        CHECK(memcmp(a, b, sizeof(a)) == 0);
    }
    /* Pitch control must not reset phase or the ongoing fade. */
    memset(&v, 0, sizeof(v)); CHECK(sb_audio_triangle_start(&v, 220));
    sb_audio_triangle_render(&v, a, 73);
    double phase = v.phase, gain = v.envelope.level;
    unsigned attack = v.envelope.elapsed;
    CHECK(sb_audio_triangle_pitch(&v, 660));
    CHECK(v.phase == phase && v.envelope.level == gain && v.envelope.elapsed == attack);
    CHECK(!sb_audio_triangle_pitch(&v, NAN) && v.frequency == 660);
    CHECK(!sb_audio_triangle_pitch(&v, 20000) && v.frequency == 660);
    sb_audio_triangle_render(&v, a, 1);
    CHECK(fabs(a[0] - 0.5*sqrt(0.5)*(73.0/480)*(1-4*fabs(phase-0.5))) < 0.000001);
    /* Stopping during attack fades from the current level, not full volume. */
    gain = v.envelope.level;
    sb_audio_triangle_stop(&v);
    sb_audio_triangle_render(&v, a, 100);
    unsigned remaining = v.stop_left;
    sb_audio_triangle_stop(&v); CHECK(v.stop_left == remaining);
    sb_audio_triangle_render(&v, a+200, 380);
    CHECK(!v.active);
    for (unsigned i = 0; i < 480; ++i)
        CHECK(fabsf(a[2*i]) <= gain*(480-i)/480 + 0.000001);
    sb_audio_triangle_render(&v, a, 100);
    for (unsigned i = 0; i < 200; ++i) CHECK(a[i] == 0);
    CHECK(sb_audio_triangle_start(&v, 440));
    sb_audio_triangle_stop(&v); sb_audio_triangle_render(&v, a, 480);
    CHECK(!v.active);
    for (unsigned i = 0; i < 960; ++i) CHECK(a[i] == 0);
    /* Capture settings for the current note; validate updates atomically. */
    memset(&v, 0, sizeof(v));
    sb_envelope_settings_t first = {4.0/48000, 4.0/48000, 0.5, 4.0/48000};
    CHECK(sb_audio_triangle_configure(&v, &first));
    CHECK(sb_audio_triangle_start(&v, 440));
    sb_audio_triangle_render(&v, a, 6);
    CHECK(fabs(v.envelope.level - 0.75) < 1e-12);
    sb_envelope_settings_t next = {8.0/48000, 0, 0.25, 10};
    CHECK(sb_audio_triangle_configure(&v, &next));
    unsigned char snapshot[sizeof(v)]; memcpy(snapshot, &v, sizeof(v));
    sb_envelope_settings_t invalid = {0.1, NAN, 0.4, 0.2};
    CHECK(!sb_audio_triangle_configure(&v, &invalid));
    CHECK(memcmp(snapshot, &v, sizeof(v)) == 0);
    CHECK(!sb_audio_triangle_start(&v, INFINITY));
    CHECK(memcmp(snapshot, &v, sizeof(v)) == 0);
    sb_audio_triangle_note_off(&v);
    CHECK(v.envelope.release_frames == 4); /* Not the newly configured 10 s. */
    sb_audio_triangle_render(&v, a, 2);
    CHECK(fabs(v.envelope.level - 0.375) < 1e-12);
    phase = v.phase;
    CHECK(sb_audio_triangle_start(&v, 660));
    CHECK(v.phase == phase && v.envelope.level == 0.375);
    CHECK(v.envelope.attack_frames == 8 && v.envelope.elapsed == 0);
    CHECK(v.envelope.release_frames == 480000);
    sb_audio_triangle_render(&v, a, 4);
    CHECK(fabs(v.envelope.level - 0.6875) < 1e-12);
    CHECK(v.envelope.stage == SB_ENVELOPE_ATTACK);
    sb_audio_triangle_render(&v, a, 4);
    CHECK(v.envelope.level == 0.25 && v.envelope.stage == SB_ENVELOPE_SUSTAIN);
    /* Stop ignores the captured ten-second release and lasts exactly 480 frames. */
    sb_audio_triangle_note_off(&v);
    sb_audio_triangle_stop(&v);
    sb_audio_triangle_render(&v, a, 240);
    CHECK(fabs(v.envelope.level - 0.125) < 1e-12);
    sb_audio_triangle_note_off(&v); sb_audio_triangle_stop(&v);
    CHECK(v.stop_left == 240);
    phase = v.phase;
    CHECK(sb_audio_triangle_start(&v, 220)); /* Retrigger during short stop too. */
    CHECK(v.phase == phase && v.envelope.level == 0.125 && !v.stopping);
    sb_audio_triangle_stop(&v);
    sb_audio_triangle_render(&v, a, 479); CHECK(v.active);
    sb_audio_triangle_render(&v, a, 1); CHECK(!v.active);
    /* Retrigger at level one holds for the entire new attack duration. */
    next = (sb_envelope_settings_t){0, 0, 1, 0};
    CHECK(sb_audio_triangle_configure(&v, &next) && sb_audio_triangle_start(&v, 440));
    next = (sb_envelope_settings_t){4.0/48000, 4.0/48000, 0.5, 0};
    CHECK(sb_audio_triangle_configure(&v, &next) && sb_audio_triangle_start(&v, 440));
    sb_audio_triangle_render(&v, a, 3);
    CHECK(v.envelope.level == 1 && v.envelope.stage == SB_ENVELOPE_ATTACK);
    sb_audio_triangle_render(&v, a, 1);
    CHECK(v.envelope.level == 1 && v.envelope.stage == SB_ENVELOPE_DECAY);
    sb_audio_triangle_note_off(&v); CHECK(!v.active && v.envelope.level == 0);
    sb_audio_triangle_render(&v, a, 20);
    for (unsigned i = 0; i < 40; ++i) CHECK(a[i] == 0);
    puts("Triangle sample checks passed");
    return 0;
}
