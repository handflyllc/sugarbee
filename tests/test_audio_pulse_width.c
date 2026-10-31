#include "../src/sb_audio_pulse.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Pulse width line %d: %s\n", __LINE__, #e); return 1; } } while (0)
#define N 2048u
#define OVERSAMPLE 32u
static float output[N], split[N], raw[N], reference[N*OVERSAMPLE];
static const double pi = 3.14159265358979323846;
static double trajectory(double t) {
    if (t < 480) return .05+.9*t/480;
    if (t < 1024) return .95;
    if (t < 1504) return .95-.9*(t-1024)/480;
    return .05;
}
static void coefficient(const float *samples, unsigned count, unsigned bin, double *re, double *im) {
    *re = *im = 0;
    for (unsigned i = 0; i < count; ++i) {
        double angle = 2*pi*bin*i/count;
        *re += samples[i]*cos(angle);
        *im -= samples[i]*sin(angle);
    }
    *re /= count; *im /= count;
}
int main(void) {
    sb_audio_pulse_t pulse = {0}, copy;
    CHECK(!sb_audio_pulse_width(NULL, .5));
    CHECK(!sb_audio_pulse_width(&pulse, .5));
    CHECK(sb_audio_pulse_reset(&pulse, 440, .25));
    double phase = pulse.phase;
    CHECK(sb_audio_pulse_width(&pulse, .75));
    CHECK(pulse.phase == phase && pulse.width == .25 && pulse.width_left == 480);
    CHECK(sb_audio_pulse_render(&pulse, output, 240));
    CHECK(pulse.width == .5 && pulse.width_left == 240);
    CHECK(sb_audio_pulse_width(&pulse, .75));
    CHECK(pulse.width_left == 240);
    CHECK(sb_audio_pulse_render(&pulse, output, 239));
    CHECK(pulse.width_left == 1 && pulse.width < .75);
    CHECK(sb_audio_pulse_render(&pulse, output, 1));
    CHECK(pulse.width == .75 && pulse.width_left == 0);
    CHECK(sb_audio_pulse_width(&pulse, .75) && pulse.width_left == 0);
    CHECK(sb_audio_pulse_width(&pulse, .05));
    CHECK(sb_audio_pulse_render(&pulse, output, 120));
    double current = pulse.width;
    phase = pulse.phase;
    CHECK(sb_audio_pulse_width(&pulse, .95));
    CHECK(pulse.width == current && pulse.width_from == current && pulse.width_left == 480);
    CHECK(pulse.phase == phase);
    CHECK(sb_audio_pulse_pitch(&pulse, 660));
    CHECK(pulse.width == current && pulse.width_left == 480 && pulse.phase == phase);
    copy = pulse;
    CHECK(sb_audio_pulse_render(&pulse, NULL, 0));
    CHECK(memcmp(&pulse, &copy, sizeof(pulse)) == 0);
    const double bad[] = {0, .049, .951, NAN, INFINITY};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
        CHECK(!sb_audio_pulse_width(&pulse, bad[i]));
        CHECK(memcmp(&pulse, &copy, sizeof(pulse)) == 0);
    }
    /* Partitioning must not change the sample timeline or ramp endpoint. */
    CHECK(sb_audio_pulse_render(&pulse, output, N));
    for (unsigned i = 0; i < N; i += 37)
        CHECK(sb_audio_pulse_render(&copy, split+i, N-i < 37 ? N-i : 37));
    CHECK(memcmp(output, split, sizeof(output)) == 0);
    CHECK(pulse.width == .95 && pulse.width_left == 0 && pulse.phase == copy.phase);
    CHECK(sb_audio_pulse_reset(&pulse, 220, .5));
    CHECK(pulse.width_left == 0 && pulse.width_target == .5 && pulse.phase == 0);
    /* Half an interval before/after a falling crossing gives +/-0.75 with
     * quadratic edge correction. Use relative edge speed, not pitch alone. */
    for (unsigned direction = 0; direction < 2; ++direction) {
        double from = direction ? .95 : .05, to = direction ? .05 : .95;
        CHECK(sb_audio_pulse_reset(&pulse, 220, from));
        CHECK(sb_audio_pulse_width(&pulse, to));
        double relative_step = 220.0/48000-(to-from)/480;
        pulse.phase = from-relative_step*.5;
        CHECK(sb_audio_pulse_render(&pulse, output, 2));
        CHECK(fabs(output[0]-.75) < 1e-6 && fabs(output[1]+.75) < 1e-6);
    }
    /* Extreme ramps, reversal, and varying pitch never escape width/output bounds. */
    const double frequencies[] = {220, 440, 660};
    for (unsigned f = 0; f < 3; ++f) {
        CHECK(sb_audio_pulse_reset(&pulse, frequencies[f], .05));
        for (unsigned run = 0; run < 30; ++run) {
            CHECK(sb_audio_pulse_width(&pulse, run%2 ? .05 : .95));
            unsigned frames = run%3 ? 137 : 600;
            CHECK(sb_audio_pulse_render(&pulse, output, frames));
            CHECK(pulse.width >= .05 && pulse.width <= .95);
            CHECK(pulse.previous_falling_step > 0);
            for (unsigned i = 0; i < frames; ++i) CHECK(isfinite(output[i]) && fabs(output[i]) <= 1);
        }
    }
    /* Periodic moving-width signal: compare selected complex coefficients
     * against independently generated 32x-rate raw PWM. Its low-frequency DFT
     * coefficients approximate a band-limited reference, without using BLEP.
     * Compare only common base-rate bins, not all high-rate harmonics. */
    const unsigned carrier_bins[] = {11, 19, 27};
    for (unsigned f = 0; f < 3; ++f) {
        unsigned carrier = carrier_bins[f];
        CHECK(sb_audio_pulse_reset(&pulse, carrier*48000.0/N, .05));
        for (unsigned i = 0; i < N; ++i) {
            if (i == 0) CHECK(sb_audio_pulse_width(&pulse, .95));
            if (i == 1024) CHECK(sb_audio_pulse_width(&pulse, .05));
            CHECK(fabs(pulse.width-trajectory(i)) < 1e-12);
            raw[i] = pulse.phase < trajectory(i) ? 1 : -1;
            CHECK(sb_audio_pulse_render(&pulse, output+i, 1));
        }
        for (unsigned i = 0; i < N*OVERSAMPLE; ++i) {
            double t = (double)i/OVERSAMPLE;
            double phase_ref = t*carrier/N;
            phase_ref -= floor(phase_ref);
            reference[i] = phase_ref < trajectory(t) ? 1 : -1;
        }
        double corrected_error = 0, raw_error = 0;
        for (unsigned bin = 1; bin < N/2; bin += 13) {
            double re, im, cr, ci, rr, ri;
            coefficient(reference, N*OVERSAMPLE, bin, &re, &im);
            coefficient(output, N, bin, &cr, &ci);
            coefficient(raw, N, bin, &rr, &ri);
            corrected_error += (cr-re)*(cr-re)+(ci-im)*(ci-im);
            raw_error += (rr-re)*(rr-re)+(ri-im)*(ri-im);
        }
        printf("Moving pulse %.3f Hz: selected-bin reference error reduced %.2f dB\n",
               carrier*48000.0/N, 10*log10(raw_error/corrected_error));
        CHECK(corrected_error < raw_error*.5);
        /* Preserve the carrier rather than passing by attenuating everything. */
        double re, im, cr, ci;
        coefficient(reference, N*OVERSAMPLE, carrier, &re, &im);
        coefficient(output, N, carrier, &cr, &ci);
        CHECK(hypot(cr-re, ci-im) < .01);
    }
    puts("Live pulse-width checks passed");
    return 0;
}
