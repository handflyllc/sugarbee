#include "../src/sb_audio_triangle.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Pan line %d: %s\n", __LINE__, #e); return 1; } } while (0)
#define NEAR(a,b) (fabs((a)-(b)) < 2e-7)
static float one[2], reference[2], whole[2400], pieces[2400];
int main(void) {
    sb_audio_triangle_t v = {0};
    sb_envelope_settings_t s = {0,0,1,0.1};
    CHECK(sb_audio_triangle_configure(&v, &s) && sb_audio_triangle_start(&v, 440));
    CHECK(v.volume == .5 && v.pan == 0);
    sb_audio_triangle_stop(&v); sb_audio_triangle_render(&v, whole, 480);
    /* Exact end-channel silence and centered equal-power gains. */
    const double positions[] = {-1,-.5,0,.5,1};
    for (unsigned p = 0; p < 5; ++p) {
        CHECK(sb_audio_triangle_volume(&v, 1) && sb_audio_triangle_pan(&v, positions[p]));
        CHECK(sb_audio_triangle_start(&v, 440));
        for (unsigned i = 0; i < 120; ++i) {
            double raw = 1-4*fabs(v.phase-.5);
            sb_audio_triangle_render(&v, one, 1);
            CHECK(NEAR(one[0]*one[0]+one[1]*one[1], raw*raw));
            double angle = (positions[p]+1)*0.7853981633974483096;
            CHECK(NEAR(one[0], raw*cos(angle)) && NEAR(one[1], raw*sin(angle)));
            if (p == 0) CHECK(one[1] == 0);
            if (p == 4) CHECK(one[0] == 0);
            if (p == 2) CHECK(one[0] == one[1]);
        }
        sb_audio_triangle_stop(&v); sb_audio_triangle_render(&v, whole, 480);
    }
    CHECK(sb_audio_triangle_pan(&v, -1) && sb_audio_triangle_volume(&v, 1));
    CHECK(sb_audio_triangle_start(&v, 440));
    sb_audio_triangle_t baseline = v;
    CHECK(sb_audio_triangle_pan(&v, 1) && sb_audio_triangle_volume(&v, 0));
    for (unsigned i = 0; i < 480; ++i) {
        CHECK(NEAR(v.pan, -1+2*i/480.0) && NEAR(v.volume, 1-i/480.0));
        double raw = 1-4*fabs(v.phase-.5);
        double angle = (v.pan+1)*0.7853981633974483096;
        CHECK(sb_audio_triangle_pan(&v, 1)); /* Same target does not extend ramp. */
        sb_audio_triangle_render(&v, one, 1);
        sb_audio_triangle_render(&baseline, reference, 1);
        CHECK(NEAR(one[0], raw*(1-i/480.0)*cos(angle)));
        CHECK(NEAR(one[1], raw*(1-i/480.0)*sin(angle)));
        CHECK(v.phase == baseline.phase && v.envelope.level == baseline.envelope.level);
    }
    CHECK(v.pan == 1 && v.volume == 0 && !v.pan_left && !v.volume_left);
    for (unsigned i = 0; i < 1000; ++i) {
        sb_audio_triangle_render(&v, one, 1);
        sb_audio_triangle_render(&baseline, reference, 1);
        CHECK(one[0] == 0 && one[1] == 0 && v.active);
        CHECK(v.phase == baseline.phase && v.envelope.elapsed == baseline.envelope.elapsed);
    }
    CHECK(sb_audio_triangle_volume(&v, 1));
    sb_audio_triangle_render(&v, whole, 240);
    CHECK(NEAR(v.volume, .5));
    CHECK(sb_audio_triangle_volume(&v, .2) && v.volume == .5);
    sb_audio_triangle_render(&v, whole, 480); CHECK(v.volume == .2);
    unsigned char saved[sizeof(v)]; memcpy(saved, &v, sizeof(v));
    const double bad[] = {NAN,INFINITY,-INFINITY,1.01,-1.01};
    for (unsigned i = 0; i < 5; ++i) {
        CHECK(!sb_audio_triangle_pan(&v, bad[i]));
        CHECK(!sb_audio_triangle_volume(&v, bad[i]));
        CHECK(memcmp(saved, &v, sizeof(v)) == 0);
    }
    CHECK(!sb_audio_triangle_volume(&v, -.1));
    CHECK(memcmp(saved, &v, sizeof(v)) == 0);
    CHECK(!sb_audio_triangle_volume(NULL, .5) && !sb_audio_triangle_pan(NULL, 0));
    /* Buffer partitioning cannot change ramps, including through release. */
    CHECK(sb_audio_triangle_volume(&v, .8) && sb_audio_triangle_pan(&v, -.7));
    sb_audio_triangle_note_off(&v);
    baseline = v;
    sb_audio_triangle_render(&v, whole, 1200);
    for (unsigned i = 0; i < 1200; i += 37) {
        unsigned n = i+37 <= 1200 ? 37 : 1200-i;
        sb_audio_triangle_render(&baseline, pieces+2*i, n);
    }
    CHECK(memcmp(whole, pieces, sizeof(whole)) == 0);
    CHECK(v.phase == baseline.phase && v.envelope.level == baseline.envelope.level);
    /* Retrigger preserves live controls and ramps. Idle restart keeps targets. */
    CHECK(sb_audio_triangle_pan(&v, 1));
    sb_audio_triangle_render(&v, one, 1);
    double pan = v.pan; unsigned left = v.pan_left;
    CHECK(sb_audio_triangle_start(&v, 660) && v.pan == pan && v.pan_left == left);
    sb_audio_triangle_stop(&v); sb_audio_triangle_render(&v, whole, 480);
    CHECK(sb_audio_triangle_start(&v, 440) && v.pan == 1 && v.volume == .8);
    /* Muted attack/release still advance identically to an audible reference. */
    memset(&v, 0, sizeof(v));
    s = (sb_envelope_settings_t){.1,.1,.7,.1};
    CHECK(sb_audio_triangle_configure(&v, &s));
    CHECK(sb_audio_triangle_volume(&v, 0) && sb_audio_triangle_start(&v, 440));
    baseline = v;
    CHECK(sb_audio_triangle_volume(&baseline, 1));
    for (unsigned i = 0; i < 1000; ++i) {
        sb_audio_triangle_render(&v, one, 1);
        sb_audio_triangle_render(&baseline, reference, 1);
        CHECK(one[0] == 0 && one[1] == 0);
        CHECK(v.phase == baseline.phase && v.envelope.level == baseline.envelope.level);
    }
    CHECK(v.envelope.level > 0);
    sb_audio_triangle_note_off(&v); sb_audio_triangle_note_off(&baseline);
    for (unsigned i = 0; i < 4800; ++i) {
        sb_audio_triangle_render(&v, one, 1);
        sb_audio_triangle_render(&baseline, reference, 1);
        CHECK(v.envelope.level == baseline.envelope.level && v.phase == baseline.phase);
    }
    CHECK(!v.active && !baseline.active);
    puts("Volume/pan sample checks passed");
    return 0;
}
