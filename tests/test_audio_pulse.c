#include "../src/sb_audio_pulse.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Pulse line %d: %s\n", __LINE__, #e); return 1; } } while (0)
#define N 8192u
static float output[N], split[N], raw[N];
static const double pi = 3.14159265358979323846;
/* Coherent frequencies place intended harmonics on exact DFT bins. Parseval
 * minus their energy measures energy outside the wanted harmonic bins. This
 * includes aliased content but is not a complete all-frequency alias metric. */
static double unwanted(const float *samples, unsigned fundamental, double *amplitude) {
    double total = 0, mean = 0, wanted = 0;
    for (unsigned i = 0; i < N; ++i) {
        total += (double)samples[i]*samples[i];
        mean += samples[i];
    }
    total /= N; mean /= N;
    for (unsigned bin = fundamental; bin < N/2; bin += fundamental) {
        double re = 0, im = 0;
        for (unsigned i = 0; i < N; ++i) {
            double angle = 2*pi*bin*i/N;
            re += samples[i]*cos(angle);
            im -= samples[i]*sin(angle);
        }
        re /= N; im /= N;
        double power = re*re + im*im;
        wanted += 2*power;
        if (bin == fundamental) *amplitude = 2*sqrt(power);
    }
    return total - mean*mean - wanted;
}
int main(void) {
    sb_audio_pulse_t pulse = {0}, copy;
    float sentinel = 7;
    CHECK(!sb_audio_pulse_render(&pulse, &sentinel, 1));
    CHECK(!sb_audio_pulse_pitch(&pulse, 440));
    CHECK(!sb_audio_pulse_reset(NULL, 440, .5));
    CHECK(!sb_audio_pulse_render(NULL, &sentinel, 1));
    CHECK(!sb_audio_pulse_pitch(NULL, 440));
    CHECK(sb_audio_pulse_reset(&pulse, 440, SB_AUDIO_PULSE_DEFAULT_WIDTH));
    CHECK(pulse.phase == 0 && pulse.width == .5);
    copy = pulse;
    const double bad_pitch[] = {0, 219.99, 660.01, NAN, INFINITY, -INFINITY};
    const double bad_width[] = {0, .04999, .95001, 1, NAN, INFINITY, -INFINITY};
    for (unsigned i = 0; i < sizeof(bad_pitch)/sizeof(bad_pitch[0]); ++i) {
        CHECK(!sb_audio_pulse_reset(&pulse, bad_pitch[i], .25));
        CHECK(!sb_audio_pulse_pitch(&pulse, bad_pitch[i]));
        CHECK(pulse.phase == copy.phase && pulse.frequency == copy.frequency && pulse.width == copy.width);
    }
    for (unsigned i = 0; i < sizeof(bad_width)/sizeof(bad_width[0]); ++i) {
        CHECK(!sb_audio_pulse_reset(&pulse, 660, bad_width[i]));
        CHECK(pulse.phase == copy.phase && pulse.frequency == copy.frequency && pulse.width == copy.width);
    }
    CHECK(!sb_audio_pulse_render(&pulse, NULL, 1));
    CHECK(!sb_audio_pulse_render(&pulse, &sentinel, SIZE_MAX));
    CHECK(sb_audio_pulse_render(&pulse, NULL, 0));
    CHECK(pulse.phase == 0 && sentinel == 7);
    sb_audio_pulse_t malformed = pulse;
    malformed.phase = NAN;
    CHECK(!sb_audio_pulse_render(&malformed, &sentinel, 1));
    malformed = pulse; malformed.width = 0;
    CHECK(!sb_audio_pulse_render(&malformed, &sentinel, 1));
    CHECK(sentinel == 7);
    /* 480 Hz gives .01 phase steps. Check both edges and correction interiors. */
    CHECK(sb_audio_pulse_reset(&pulse, 480, .25));
    CHECK(sb_audio_pulse_render(&pulse, output, 101));
    CHECK(output[0] == 0 && output[1] == 1 && output[24] == 1);
    CHECK(fabs(output[25]) < 1e-6 && output[26] == -1);
    CHECK(fabs(output[100]) < 1e-6);
    const double phases[] = {.005, .995, .245, .255};
    const double values[] = {.75, -.75, .75, -.75};
    for (unsigned i = 0; i < 4; ++i) {
        pulse.phase = phases[i];
        CHECK(sb_audio_pulse_render(&pulse, &sentinel, 1));
        CHECK(fabs(sentinel-values[i]) < 1e-6);
    }
    const double widths[] = {.05, .125, .25, .5, .75, .875, .95};
    const double pitches[] = {220, 220.125, 440, 659.75, 660};
    for (unsigned w = 0; w < 7; ++w) for (unsigned f = 0; f < 5; ++f) {
        CHECK(sb_audio_pulse_reset(&pulse, pitches[f], widths[w]));
        copy = pulse;
        CHECK(sb_audio_pulse_render(&pulse, output, N));
        for (unsigned i = 0; i < N; i += 37)
            CHECK(sb_audio_pulse_render(&copy, split+i, N-i < 37 ? N-i : 37));
        CHECK(memcmp(output, split, sizeof(output)) == 0 && pulse.phase == copy.phase);
        CHECK(sb_audio_pulse_reset(&copy, pitches[f], widths[w]));
        CHECK(sb_audio_pulse_render(&copy, split, N));
        CHECK(memcmp(output, split, sizeof(output)) == 0);
        for (unsigned i = 0; i < N; ++i) CHECK(isfinite(output[i]) && fabs(output[i]) <= 1);
        double phase = pulse.phase;
        CHECK(sb_audio_pulse_pitch(&pulse, 330));
        CHECK(pulse.phase == phase && pulse.width == widths[w]);
        CHECK(sb_audio_pulse_render(&pulse, &sentinel, 1));
        double next = phase+330.0/48000;
        if (next >= 1) next -= 1;
        CHECK(pulse.phase == next);
    }
    /* Dense phase grid covers both minimum-duty extremes at maximum pitch. */
    for (unsigned w = 0; w < 7; ++w) {
        CHECK(sb_audio_pulse_reset(&pulse, 660, widths[w]));
        for (unsigned i = 0; i < 10001; ++i) {
            pulse.phase = i/10001.0;
            CHECK(sb_audio_pulse_render(&pulse, &sentinel, 1));
            CHECK(isfinite(sentinel) && fabs(sentinel) <= 1);
        }
    }
    /* Coherent records check DC independently of AC alias-reduction metrics. */
    const unsigned bins[] = {39, 75, 111};
    const double spectral_widths[] = {.05, .25, .5, .75, .95};
    for (unsigned f = 0; f < 3; ++f) for (unsigned w = 0; w < 5; ++w) {
        unsigned bin = bins[f];
        double frequency = bin*48000.0/N, width = spectral_widths[w];
        CHECK(sb_audio_pulse_reset(&pulse, frequency, width));
        CHECK(sb_audio_pulse_render(&pulse, output, N));
        double phase = 0, mean = 0;
        for (unsigned i = 0; i < N; ++i) {
            raw[i] = phase < width ? 1 : -1;
            phase += (double)bin/N;
            if (phase >= 1) phase -= 1;
            mean += output[i];
        }
        CHECK(fabs(mean/N - (2*width-1)) < 1e-5);
        double amp = 0, raw_amp = 0;
        double error = unwanted(output, bin, &amp), raw_error = unwanted(raw, bin, &raw_amp);
        CHECK(error > 0 && raw_error > 0);
        CHECK(error < raw_error*.1);
        CHECK(amp/raw_amp > .99 && amp/raw_amp < 1.01);
        CHECK(fabs(amp - 4/pi*sin(pi*width)) < .01);
        printf("Pulse %.6f Hz width %.2f: unwanted AC power reduced %.2f dB\n",
               frequency, width, 10*log10(raw_error/error));
    }
    puts("Standalone fixed-width pulse checks passed");
    return 0;
}
