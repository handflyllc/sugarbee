#include "sb_audio_triangle.h"
#include "sb_audio_tone.h"
#include <math.h>

#define FADE_FRAMES 480u
static bool valid_pitch(double frequency) {
    return isfinite(frequency) && frequency >= 220.0 && frequency <= 660.0;
}

bool sb_audio_triangle_start(sb_audio_triangle_t *voice, double frequency) {
    if (!valid_pitch(frequency) || voice->active) return false;
    *voice = (sb_audio_triangle_t){0};
    voice->frequency = frequency;
    voice->phase = 0.25; /* Zero crossing, not the negative peak. */
    voice->active = true;
    return true;
}

bool sb_audio_triangle_pitch(sb_audio_triangle_t *voice, double frequency) {
    if (!valid_pitch(frequency) || !voice->active) return false;
    voice->frequency = frequency; /* Phase and fade state stay untouched. */
    return true;
}

void sb_audio_triangle_stop(sb_audio_triangle_t *voice) {
    if (!voice->active || voice->stopping) return;
    voice->stopping = true;
    voice->release_gain = voice->gain;
    voice->release_left = FADE_FRAMES;
}

void sb_audio_triangle_render(sb_audio_triangle_t *voice, float *output, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        float sample = 0;
        if (voice->active) {
            if (voice->stopping) {
                voice->gain = voice->release_gain * voice->release_left / FADE_FRAMES;
            } else {
                voice->gain = (double)voice->attack_frame / FADE_FRAMES;
                if (voice->attack_frame < FADE_FRAMES) ++voice->attack_frame;
            }
            double triangle = 1.0 - 4.0 * fabs(voice->phase - 0.5);
            sample = (float)(0.05 * voice->gain * triangle);
            voice->phase += voice->frequency / SB_AUDIO_RATE;
            if (voice->phase >= 1.0) voice->phase -= 1.0;
            if (voice->stopping && --voice->release_left == 0) {
                voice->active = false;
                voice->gain = 0;
            }
        }
        output[2*i] = sample;
        output[2*i+1] = sample;
    }
}
