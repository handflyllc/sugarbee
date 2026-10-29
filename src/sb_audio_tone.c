#include "sb_audio_tone.h"
#include <math.h>

void sb_audio_tone_start(sb_audio_tone_t *tone) {
    tone->frame = 0;
    tone->active = true;
}

void sb_audio_tone_render(sb_audio_tone_t *tone, float *output, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        float sample = 0.0f;
        if (tone->active) {
            unsigned frame = tone->frame;
            /* Ten-ms linear fades, entirely within the one-second duration.
               First and final samples are zero. Absolute frame position keeps
               phase continuous regardless of callback buffer boundaries. */
            double gain = 1.0;
            if (frame < SB_AUDIO_TONE_FADE_FRAMES)
                gain = (double)frame / SB_AUDIO_TONE_FADE_FRAMES;
            unsigned remaining = SB_AUDIO_TONE_FRAMES - 1u - frame;
            if (remaining < SB_AUDIO_TONE_FADE_FRAMES)
                gain = (double)remaining / SB_AUDIO_TONE_FADE_FRAMES;
            double phase = 6.2831853071795864769 * 440.0 * frame / SB_AUDIO_RATE;
            sample = (float)(0.05 * gain * sin(phase));
            if (++tone->frame == SB_AUDIO_TONE_FRAMES) tone->active = false;
        }
        output[2 * i] = sample;
        output[2 * i + 1] = sample;
    }
}
