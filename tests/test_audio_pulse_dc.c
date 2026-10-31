#include "../src/sb_audio_pulse.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Pulse DC line %d: %s\n", __LINE__, #e); return 1; } } while (0)
#define N 8192u
static float output[N], raw[N], split[N];
int main(void) {
    sb_audio_pulse_t pulse = {0}, reference, copy;
    float sentinel = 123;
    CHECK(!sb_audio_pulse_render_conditioned(NULL, &sentinel, 1));
    CHECK(!sb_audio_pulse_render_conditioned(&pulse, &sentinel, 1));
    CHECK(sentinel == 123);
    CHECK(sb_audio_pulse_reset(&pulse, 440, .5));
    copy = pulse;
    CHECK(sb_audio_pulse_render_conditioned(&pulse, NULL, 0));
    CHECK(!sb_audio_pulse_render_conditioned(&pulse, NULL, 1));
    CHECK(!sb_audio_pulse_render_conditioned(&pulse, &sentinel, SIZE_MAX/sizeof(float)+1));
    CHECK(memcmp(&pulse, &copy, sizeof(pulse)) == 0 && sentinel == 123);
    const double widths[] = {.05, .25, .5, .75, .95};
    const unsigned bins[] = {39, 75, 111};
    for (unsigned f = 0; f < 3; ++f) {
        for (unsigned w = 0; w < 5; ++w) {
            CHECK(sb_audio_pulse_reset(&pulse, bins[f]*48000.0/N, widths[w]));
            reference = pulse;
            CHECK(sb_audio_pulse_render_conditioned(&pulse, output, N));
            CHECK(sb_audio_pulse_render(&reference, raw, N));
            double mean = 0, raw_mean = 0;
            CHECK(fabs(output[0]-(.5-widths[w])) < 1e-7);
            for (unsigned i = 0; i < N; ++i) {
                CHECK(isfinite(output[i]) && fabs(output[i]) <= .9500001);
                CHECK(fabs(output[i]-.5*(raw[i]-(2*widths[w]-1))) < 1e-7);
                mean += output[i]; raw_mean += raw[i];
            }
            CHECK(fabs(mean/N) < 1e-5);
            CHECK(fabs(raw_mean/N-(2*widths[w]-1)) < 1e-5);
            CHECK(pulse.phase == reference.phase && pulse.width == reference.width);
        }
    }
    /* Compare paired states sample by sample through full ramps and reversals.
     * This catches use of target/next width instead of the rendered width. */
    const double frequencies[] = {220, 440, 660};
    for (unsigned f = 0; f < 3; ++f) {
        CHECK(sb_audio_pulse_reset(&pulse, frequencies[f], .05));
        reference = pulse;
        for (unsigned run = 0; run < 12; ++run) {
            double target = run%2 ? .05 : .95;
            CHECK(sb_audio_pulse_width(&pulse, target));
            CHECK(sb_audio_pulse_width(&reference, target));
            unsigned count = run%3 ? 137 : 600;
            for (unsigned i = 0; i < count; ++i) {
                double width = reference.width;
                CHECK(sb_audio_pulse_render(&reference, raw, 1));
                CHECK(sb_audio_pulse_render_conditioned(&pulse, output, 1));
                CHECK(fabs(output[0]-.5*(raw[0]-(2*width-1))) < 1e-7);
                CHECK(isfinite(output[0]) && fabs(output[0]) <= .9500001);
                CHECK(pulse.phase == reference.phase && pulse.width == reference.width);
            }
        }
    }
    CHECK(sb_audio_pulse_width(&pulse, .5));
    copy = pulse;
    CHECK(sb_audio_pulse_render_conditioned(&pulse, output, N));
    for (unsigned i = 0; i < N; i += 37)
        CHECK(sb_audio_pulse_render_conditioned(&copy, split+i, N-i < 37 ? N-i : 37));
    CHECK(memcmp(output, split, sizeof(output)) == 0);
    CHECK(pulse.phase == copy.phase && pulse.width == copy.width);
    /* Reset cancels the ramp and reproduces conditioned output exactly. */
    CHECK(sb_audio_pulse_reset(&pulse, 440, .25));
    CHECK(sb_audio_pulse_render_conditioned(&pulse, output, N));
    CHECK(sb_audio_pulse_reset(&pulse, 440, .25));
    CHECK(sb_audio_pulse_render_conditioned(&pulse, split, N));
    CHECK(memcmp(output, split, sizeof(output)) == 0);
    puts("Pulse DC conditioning checks passed");
    return 0;
}
