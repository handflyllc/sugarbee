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
    CHECK(!sb_audio_triangle_start(&v, 219));
    CHECK(!sb_audio_triangle_start(&v, 661));
    sb_audio_triangle_stop(&v);
    sb_audio_triangle_render(&v, a, 10);
    for (unsigned i = 0; i < 20; ++i) CHECK(a[i] == 0);
    for (unsigned hz = 220; hz <= 660; hz += 220) {
        memset(&v, 0, sizeof(v));
        CHECK(sb_audio_triangle_start(&v, hz));
        CHECK(!sb_audio_triangle_start(&v, hz));
        sb_audio_triangle_t chunked = v;
        sb_audio_triangle_render(&v, a, 48000);
        unsigned crossings = 0;
        float peak = 0;
        for (unsigned i = 0; i < 48000; ++i) {
            CHECK(isfinite(a[2*i]) && fabsf(a[2*i]) <= 0.050001f);
            CHECK(a[2*i] == a[2*i+1]);
            if (fabsf(a[2*i]) > peak) peak = fabsf(a[2*i]);
            if (i && a[2*(i-1)] <= 0 && a[2*i] > 0) ++crossings;
            if (i < 480) CHECK(fabsf(a[2*i]) <= 0.05f*i/480 + 0.000001f);
        }
        CHECK(crossings == hz && peak > 0.049f);
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
    double phase = v.phase, gain = v.gain;
    unsigned attack = v.attack_frame;
    CHECK(sb_audio_triangle_pitch(&v, 660));
    CHECK(v.phase == phase && v.gain == gain && v.attack_frame == attack);
    CHECK(!sb_audio_triangle_pitch(&v, NAN) && v.frequency == 660);
    CHECK(!sb_audio_triangle_pitch(&v, 20000) && v.frequency == 660);
    sb_audio_triangle_render(&v, a, 1);
    CHECK(fabs(a[0] - 0.05*(73.0/480)*(1-4*fabs(phase-0.5))) < 0.000001);
    /* Stopping during attack fades from the current level, not full volume. */
    gain = v.gain;
    sb_audio_triangle_stop(&v);
    sb_audio_triangle_render(&v, a, 100);
    unsigned remaining = v.release_left;
    sb_audio_triangle_stop(&v); CHECK(v.release_left == remaining);
    sb_audio_triangle_render(&v, a+200, 380);
    CHECK(!v.active);
    for (unsigned i = 0; i < 480; ++i)
        CHECK(fabsf(a[2*i]) <= 0.05*gain*(480-i)/480 + 0.000001);
    sb_audio_triangle_render(&v, a, 100);
    for (unsigned i = 0; i < 200; ++i) CHECK(a[i] == 0);
    CHECK(sb_audio_triangle_start(&v, 440));
    sb_audio_triangle_stop(&v); sb_audio_triangle_render(&v, a, 480);
    CHECK(!v.active);
    for (unsigned i = 0; i < 960; ++i) CHECK(a[i] == 0);
    puts("Triangle sample checks passed");
    return 0;
}
