#include "../src/sb_audio_saw.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Saw line %d: %s\n", __LINE__, #e); return 1; } } while (0)
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
    sb_audio_saw_t saw = {0}, copy;
    float sentinel = 7;
    CHECK(!sb_audio_saw_render(&saw, &sentinel, 1));
    CHECK(!sb_audio_saw_pitch(&saw, 440));
    CHECK(!sb_audio_saw_reset(NULL, 440));
    CHECK(!sb_audio_saw_render(NULL, &sentinel, 1));
    CHECK(!sb_audio_saw_pitch(NULL, 440));
    CHECK(sb_audio_saw_reset(&saw, 440));
    CHECK(saw.phase == .5 && saw.frequency == 440);
    copy = saw;
    const double invalid[] = {0, -1, 219.99, 660.01, NAN, INFINITY, -INFINITY};
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        CHECK(!sb_audio_saw_reset(&saw, invalid[i]));
        CHECK(!sb_audio_saw_pitch(&saw, invalid[i]));
        CHECK(saw.phase == copy.phase && saw.frequency == copy.frequency);
    }
    CHECK(!sb_audio_saw_render(&saw, NULL, 1));
    CHECK(!sb_audio_saw_render(&saw, &sentinel, SIZE_MAX));
    CHECK(sb_audio_saw_render(&saw, NULL, 0));
    CHECK(saw.phase == copy.phase && saw.frequency == copy.frequency && sentinel == 7);
    sb_audio_saw_t malformed = {.phase = NAN, .frequency = 440};
    CHECK(!sb_audio_saw_render(&malformed, &sentinel, 1) && sentinel == 7);
    malformed.phase = 1;
    CHECK(!sb_audio_saw_pitch(&malformed, 440));
    /* At 480 Hz the phase increment is .01: known rising ramp then wrap. */
    CHECK(sb_audio_saw_reset(&saw, 480));
    CHECK(sb_audio_saw_render(&saw, output, 101));
    CHECK(output[0] == 0);
    CHECK(fabs(output[1]-.02) < 1e-7);
    CHECK(fabs(output[49]-.98) < 1e-7);
    CHECK(fabs(output[50]) < 1e-7);
    CHECK(fabs(output[51]+.98) < 1e-7);
    CHECK(fabs(output[100]) < 1e-7);
    /* Interior correction samples differ from the raw discontinuity. */
    saw.phase = .995;
    CHECK(sb_audio_saw_render(&saw, output, 1));
    CHECK(fabs(output[0]-.74) < 1e-7);
    saw.phase = .005;
    CHECK(sb_audio_saw_render(&saw, output, 1));
    CHECK(fabs(output[0]+.74) < 1e-7);
    const double pitches[] = {220, 220.125, 440, 659.75, 660};
    for (unsigned p = 0; p < 5; ++p) {
        CHECK(sb_audio_saw_reset(&saw, pitches[p]));
        copy = saw;
        CHECK(sb_audio_saw_render(&saw, output, N));
        for (unsigned i = 0; i < N; i += 37)
            CHECK(sb_audio_saw_render(&copy, split+i, N-i < 37 ? N-i : 37));
        CHECK(memcmp(output, split, sizeof(output)) == 0 && saw.phase == copy.phase);
        CHECK(sb_audio_saw_reset(&copy, pitches[p]));
        CHECK(sb_audio_saw_render(&copy, split, N));
        CHECK(memcmp(output, split, sizeof(output)) == 0);
        for (unsigned i = 0; i < N; ++i) CHECK(isfinite(output[i]) && fabs(output[i]) <= 1);
        CHECK(saw.phase >= 0 && saw.phase < 1);
        double phase = saw.phase;
        CHECK(sb_audio_saw_pitch(&saw, 330));
        CHECK(saw.phase == phase);
        CHECK(sb_audio_saw_render(&saw, output, 1));
        double next = phase + 330.0/48000;
        if (next >= 1) next -= 1;
        CHECK(saw.phase == next);
    }
    /* Sweep phase and supported pitch including correction boundaries. */
    for (unsigned hz = 220; hz <= 660; hz += 20) {
        CHECK(sb_audio_saw_reset(&saw, hz));
        for (unsigned i = 0; i <= 1000; ++i) {
            saw.phase = i/1001.0;
            CHECK(sb_audio_saw_render(&saw, &sentinel, 1));
            CHECK(isfinite(sentinel) && fabs(sentinel) <= 1);
        }
    }
    const unsigned bins[] = {39, 75, 111};
    for (unsigned k = 0; k < 3; ++k) {
        unsigned bin = bins[k];
        double frequency = bin*48000.0/N;
        CHECK(sb_audio_saw_reset(&saw, frequency));
        CHECK(sb_audio_saw_render(&saw, output, N));
        double phase = .5;
        for (unsigned i = 0; i < N; ++i) {
            raw[i] = (float)(2*phase-1);
            phase += (double)bin/N;
            if (phase >= 1) phase -= 1;
        }
        double corrected_amp = 0, raw_amp = 0;
        double corrected_error = unwanted(output, bin, &corrected_amp);
        double raw_error = unwanted(raw, bin, &raw_amp);
        CHECK(raw_error > 0 && corrected_error > 0);
        /* At least 10 dB less unwanted power, without just lowering volume. */
        CHECK(corrected_error < raw_error*.1);
        CHECK(corrected_amp/raw_amp > .99 && corrected_amp/raw_amp < 1.01);
        CHECK(fabs(corrected_amp - 2/pi) < .01);
        printf("Saw %.6f Hz: unwanted power reduced %.2f dB\n",
               frequency, 10*log10(raw_error/corrected_error));
    }
    puts("Standalone saw checks passed");
    return 0;
}
