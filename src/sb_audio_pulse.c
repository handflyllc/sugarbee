#include "sb_audio_pitch.h"
#include "sb_audio_pulse.h"
#include <math.h>
#include <stdint.h>
#include <float.h>

static bool valid_frequency(double frequency) {
    return sb_audio_pitch_valid(frequency);
}
static bool valid_width(double width) {
    return isfinite(width) && width >= .05 && width <= .95;
}
/* History may belong to the previous pitch. Falling motion additionally
 * includes up to 0.9/480 cycles of width movement in either direction.
 * Its bound allows floating-point ramp arithmetic error, not pitch tolerance.
 * At the maximum pitch all motion remains below one cycle per sample. */
#define MIN_STEP (SB_AUDIO_PITCH_MIN / SB_AUDIO_PULSE_RATE)
#define MAX_STEP (SB_AUDIO_PITCH_MAX / SB_AUDIO_PULSE_RATE)
#define MAX_WIDTH_STEP (.9 / SB_AUDIO_PULSE_WIDTH_FRAMES)
#define HISTORY_ROUNDOFF (16 * DBL_EPSILON)
static bool valid_state(const sb_audio_pulse_t *pulse) {
    return pulse && valid_frequency(pulse->frequency) && valid_width(pulse->width) &&
           isfinite(pulse->phase) && pulse->phase >= 0 && pulse->phase < 1 &&
           valid_width(pulse->width_from) && valid_width(pulse->width_target) &&
           pulse->width_left <= SB_AUDIO_PULSE_WIDTH_FRAMES &&
           isfinite(pulse->previous_step) && pulse->previous_step >= MIN_STEP && pulse->previous_step <= MAX_STEP &&
           isfinite(pulse->previous_falling_step) && pulse->previous_falling_step >= MIN_STEP-MAX_WIDTH_STEP-HISTORY_ROUNDOFF &&
           pulse->previous_falling_step <= MAX_STEP+MAX_WIDTH_STEP+HISTORY_ROUNDOFF;
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
/* Sum past and future residuals independently. A moving falling edge may
 * reverse or stop, and narrow pulses may overlap rising/falling corrections.
 * Zero motion needs no correction on that side; never divide by zero. */
static double rising_residual(double phase, double previous, double next) {
    double value = 0, d;
    if (previous > 0 && phase < previous) {
        d = 1-phase/previous; value -= d*d;
    }
    if (previous < 0 && phase > 1+previous) {
        d = 1+(phase-1)/(-previous); value += d*d;
    }
    if (next > 0 && phase > 1-next) {
        d = 1+(phase-1)/next; value += d*d;
    }
    if (next < 0 && phase < -next) {
        d = 1-phase/(-next); value -= d*d;
    }
    return value;
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
        /* Relative falling-edge motion includes signed width velocity.
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
