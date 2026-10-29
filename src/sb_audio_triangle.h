/* Private prototype: fixed quiet volume, centered stereo, 220--660 Hz only. */
#ifndef SB_AUDIO_TRIANGLE_H
#define SB_AUDIO_TRIANGLE_H
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    double phase, frequency, gain, release_gain;
    unsigned attack_frame, release_left;
    bool active, stopping;
} sb_audio_triangle_t;

bool sb_audio_triangle_start(sb_audio_triangle_t *voice, double frequency);
bool sb_audio_triangle_pitch(sb_audio_triangle_t *voice, double frequency);
void sb_audio_triangle_stop(sb_audio_triangle_t *voice);
void sb_audio_triangle_render(sb_audio_triangle_t *voice, float *output, size_t frames);
#endif
