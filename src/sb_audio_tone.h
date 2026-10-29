/* Private fixed diagnostic oscillator, not the future synthesizer API. */
#ifndef SB_AUDIO_TONE_H
#define SB_AUDIO_TONE_H
#include <stddef.h>
#include <stdbool.h>
#define SB_AUDIO_RATE 48000u
#define SB_AUDIO_TONE_FRAMES 48000u
#define SB_AUDIO_TONE_FADE_FRAMES 480u

typedef struct {
    unsigned frame;
    bool active;
} sb_audio_tone_t;

void sb_audio_tone_start(sb_audio_tone_t *tone);
/* Fill interleaved stereo frames, including silence after the tone ends. */
void sb_audio_tone_render(sb_audio_tone_t *tone, float *output, size_t frames);
#endif
