#include "sb_audio_pulse.h"
#include <math.h>
#include <stdint.h>

static bool valid_frequency(double frequency) {
    return isfinite(frequency) && frequency >= 220.0 && frequency <= 660.0;
}
static bool valid_width(double width) {
    return isfinite(width) && width >= .05 && width <= .95;
}
static bool valid_state(const sb_audio_pulse_t *pulse) {
    return pulse && valid_frequency(pulse->frequency) && valid_width(pulse->width) &&
           isfinite(pulse->phase) && pulse->phase >= 0 && pulse->phase < 1;
}
bool sb_audio_pulse_reset(sb_audio_pulse_t *pulse, double frequency, double width) {
    if (!pulse || !valid_frequency(frequency) || !valid_width(width)) return false;
    *pulse = (sb_audio_pulse_t){.phase = 0, .frequency = frequency, .width = width};
    return true;
}
bool sb_audio_pulse_pitch(sb_audio_pulse_t *pulse, double frequency) {
    if (!valid_state(pulse) || !valid_frequency(frequency)) return false;
    pulse->frequency = frequency;
    return true;
}
/* Residual for an upward step, with unit-period relative phase. The downward
 * edge uses its negative. Current pitch/width limits keep the edge regions
 * disjoint; that bound must be revisited before widening either range. */
static double rising_residual(double phase, double step) {
    if (phase < step) {
        double distance = 1.0 - phase/step;
        return -distance*distance;
    }
    if (phase > 1.0-step) {
        double distance = 1.0 + (phase-1.0)/step;
        return distance*distance;
    }
    return 0;
}
bool sb_audio_pulse_render(sb_audio_pulse_t *pulse, float *output, size_t samples) {
    if (!valid_state(pulse) || (!output && samples) || samples > SIZE_MAX/sizeof(float))
        return false;
    double step = pulse->frequency/SB_AUDIO_PULSE_RATE;
    for (size_t i = 0; i < samples; ++i) {
        double falling_phase = pulse->phase-pulse->width;
        if (falling_phase < 0) falling_phase += 1.0;
        double value = pulse->phase < pulse->width ? 1.0 : -1.0;
        value += rising_residual(pulse->phase, step);
        value -= rising_residual(falling_phase, step);
        output[i] = (float)value;
        pulse->phase += step;
        if (pulse->phase >= 1) pulse->phase -= 1;
    }
    return true;
}
