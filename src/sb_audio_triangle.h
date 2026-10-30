/* Private prototype: quiet attenuation, native volume/pan, 220--660 Hz only. */
#ifndef SB_AUDIO_TRIANGLE_H
#define SB_AUDIO_TRIANGLE_H
#include <stdbool.h>
#include <stddef.h>
#include "sb_audio_envelope.h"

typedef struct {
    double phase, frequency, stop_start;
    unsigned stop_left;
    bool active, stopping, configured;
    sb_envelope_settings_t settings;
    sb_envelope_t envelope;
    bool controls_initialized;
    double volume, pan, volume_from, pan_from, volume_target, pan_target;
    unsigned volume_left, pan_left;
} sb_audio_triangle_t;

/* Full settings update, captured only on the next start/retrigger. */
bool sb_audio_triangle_configure(sb_audio_triangle_t *voice, const sb_envelope_settings_t *settings);
/* Volume 0..1, pan -1..1; live changes ramp for 480 frames. Idle changes
 * apply immediately. Repeated identical targets do not restart a ramp. */
bool sb_audio_triangle_volume(sb_audio_triangle_t *voice, double volume);
bool sb_audio_triangle_pan(sb_audio_triangle_t *voice, double pan);
void sb_audio_triangle_note_off(sb_audio_triangle_t *voice);
bool sb_audio_triangle_start(sb_audio_triangle_t *voice, double frequency);
bool sb_audio_triangle_pitch(sb_audio_triangle_t *voice, double frequency);
void sb_audio_triangle_stop(sb_audio_triangle_t *voice);
void sb_audio_triangle_render(sb_audio_triangle_t *voice, float *output, size_t frames);
#endif
