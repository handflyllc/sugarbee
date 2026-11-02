#include "sb_audio_pulse.h"
#include <math.h>
#include <stdint.h>

static bool valid_frequency(double frequency) {
    return isfinite(frequency) && frequency >= 110.0 && frequency <= 880.0;
}
static bool valid_width(double width) {
    return isfinite(width) && width >= .05 && width <= .95;
}
/* At 110..880 Hz, step is 0.0022917..0.0183334. Width velocity is
 * bounded by 0.9/480 = 0.001875, so falling motion stays positive and below
 * 0.020209. Require each stored interval below half the minimum edge spacing
 * (0.05/2): even across pitch edits the two correction regions stay disjoint. */
static bool valid_state(const sb_audio_pulse_t *pulse) {
    return pulse && valid_frequency(pulse->frequency) && valid_width(pulse->width) &&
           isfinite(pulse->phase) && pulse->phase >= 0 && pulse->phase < 1 &&
           valid_width(pulse->width_from) && valid_width(pulse->width_target) &&
           pulse->width_left <= SB_AUDIO_PULSE_WIDTH_FRAMES &&
           isfinite(pulse->previous_step) && pulse->previous_step > 0 && pulse->previous_step < .025 &&
           isfinite(pulse->previous_falling_step) && pulse->previous_falling_step > 0 &&
           pulse->previous_falling_step < .025;
}
bool sb_audio_pulse_reset(sb_audio_pulse_t *pulse, double frequency, double width) {
    if (!pulse || !valid_frequency(frequency) || !valid_width(width)) return false;
    *pulse = (sb_audio_pulse_t){.phase = 0, .frequency = frequency, .width = width,
        .width_from = width, .width_target = width,
        .previous_step = frequency/SB_AUDIO_PULSE_RATE,
        .previous_falling_step = frequency/SB_AUDIO_PULSE_RATE};
    return true;
}
bool sb_audio_pulse_pitch(sb_audio_pulse_t *pulse, double frequency) {
    if (!valid_state(pulse) || !valid_frequency(frequency)) return false;
    pulse->frequency = frequency;
    return true;
}
bool sb_audio_pulse_width(sb_audio_pulse_t *pulse, double width) {
    if (!valid_state(pulse) || !valid_width(width)) return false;
    if (width != pulse->width_target) {
        pulse->width_from = pulse->width;
        pulse->width_target = width;
        pulse->width_left = SB_AUDIO_PULSE_WIDTH_FRAMES;
    }
    return true;
}
/* Residual for an upward step, with unit-period relative phase. The downward
 * edge uses its negative. Current pitch/width limits keep the edge regions
 * disjoint; that bound must be revisited before widening either range. */
static double rising_residual(double phase, double previous, double next) {
    if (phase < previous) {
        double distance = 1.0 - phase/previous;
        return -distance*distance;
    }
    if (phase > 1.0-next) {
        double distance = 1.0 + (phase-1.0)/next;
        return distance*distance;
    }
    return 0;
}
static bool render(sb_audio_pulse_t *pulse, float *output, size_t samples, bool conditioned) {
    if (!valid_state(pulse) || (!output && samples) || samples > SIZE_MAX/sizeof(float))
        return false;
    double step = pulse->frequency/SB_AUDIO_PULSE_RATE;
    for (size_t i = 0; i < samples; ++i) {
        double next_width = pulse->width;
        unsigned left = pulse->width_left;
        if (left) {
            --left;
            next_width = left ? pulse->width_from +
                (pulse->width_target-pulse->width_from)*
                (SB_AUDIO_PULSE_WIDTH_FRAMES-left)/SB_AUDIO_PULSE_WIDTH_FRAMES :
                pulse->width_target;
        }
        /* Relative falling-edge motion includes width velocity. With current
         * limits, step - width_delta stays positive and edge regions disjoint.
         * Past correction uses the prior interval; future correction uses the
         * next interval, including ramp start, completion, and retargeting. */
        double falling_step = step - (next_width-pulse->width);
        double falling_phase = pulse->phase-pulse->width;
        if (falling_phase < 0) falling_phase += 1.0;
        double value = pulse->phase < pulse->width ? 1.0 : -1.0;
        value += rising_residual(pulse->phase, pulse->previous_step, step);
        value -= rising_residual(falling_phase, pulse->previous_falling_step, falling_step);
        /* Use the width of this sample, before advancing the ramp. */
        if (conditioned) value = .5 * (value - (2*pulse->width - 1));
        output[i] = (float)value;
        pulse->phase += step;
        if (pulse->phase >= 1) pulse->phase -= 1;
        pulse->width = next_width;
        pulse->width_left = left;
        pulse->previous_step = step;
        pulse->previous_falling_step = falling_step;
    }
    return true;
}

bool sb_audio_pulse_render(sb_audio_pulse_t *pulse, float *output, size_t samples) {
    return render(pulse, output, samples, false);
}
bool sb_audio_pulse_render_conditioned(sb_audio_pulse_t *pulse, float *output, size_t samples) {
    return render(pulse, output, samples, true);
}
