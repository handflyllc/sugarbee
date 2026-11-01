#include "sb_audio_saw.h"
#include <math.h>
#include <stdint.h>

static bool valid_frequency(double frequency) {
    return isfinite(frequency) && frequency >= 110.0 && frequency <= 880.0;
}
static bool valid_state(const sb_audio_saw_t *saw) {
    return saw && valid_frequency(saw->frequency) &&
           isfinite(saw->phase) && saw->phase >= 0.0 && saw->phase < 1.0;
}
bool sb_audio_saw_reset(sb_audio_saw_t *saw, double frequency) {
    if (!saw || !valid_frequency(frequency)) return false;
    *saw = (sb_audio_saw_t){.phase = .5, .frequency = frequency};
    return true;
}
bool sb_audio_saw_pitch(sb_audio_saw_t *saw, double frequency) {
    if (!valid_state(saw) || !valid_frequency(frequency)) return false;
    saw->frequency = frequency;
    return true;
}
bool sb_audio_saw_render(sb_audio_saw_t *saw, float *output, size_t samples) {
    if (!valid_state(saw) || (!output && samples) || samples > SIZE_MAX / sizeof(float))
        return false;
    double step = saw->frequency / SB_AUDIO_SAW_RATE;
    for (size_t i = 0; i < samples; ++i) {
        double phase = saw->phase;
        double sample = 2.0 * phase - 1.0;
        /* Quadratic residual around the downward wrap. Correct within one
         * sample interval on either side; the rest is the unmodified ramp.
         * See tests/audio-saw.md for method references and measured limits. */
        if (phase < step) {
            double edge = 1.0 - phase / step;
            sample += edge * edge;
        } else if (phase > 1.0 - step) {
            double edge = 1.0 + (phase - 1.0) / step;
            sample -= edge * edge;
        }
        output[i] = (float)sample;
        saw->phase += step;
        if (saw->phase >= 1.0) saw->phase -= 1.0;
    }
    return true;
}
