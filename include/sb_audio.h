/* Host-owned audio output. No Lua or asset API yet. */
#ifndef SB_AUDIO_H
#define SB_AUDIO_H
#include <stdbool.h>

typedef struct sb_audio sb_audio_t;

/* Open default output and start supplying silence. Returns NULL and emits one
 * diagnostic on failure; the host may continue without audio. Call once during
 * host startup, not per cartridge. SDL audio initialization is owned here. */
sb_audio_t *sb_audio_create(void);

/* Close output before releasing callback resources and the SDL audio reference.
 * NULL is harmless. Call after cartridge shutdown and before SDL_Quit(). */
void sb_audio_destroy(sb_audio_t *audio);

/* Native diagnostic only: request one quiet, one-second 440 Hz tone.
 * False if unavailable or already active (does not restart). Call on the host
 * control thread, never in the audio callback. Normal host never requests it. */
bool sb_audio_start_test_tone(sb_audio_t *audio);
bool sb_audio_test_tone_active(sb_audio_t *audio);

/* Native prototype only: 220--660 Hz, peak 0.05, centered, ten-ms fades.
 * Start refuses an active triangle or diagnostic tone. Pitch changes preserve
 * phase, including during stopping; invalid/nonfinite pitches change nothing.
 * Stop is harmless when silent/already stopping. Active includes the stop fade.
 * All controls run on the host control thread. NULL is safe/unavailable.
 * This is not the future nine-voice synth API or its full frequency contract. */
bool sb_audio_start_triangle(sb_audio_t *audio, double frequency);
bool sb_audio_set_triangle_pitch(sb_audio_t *audio, double frequency);
void sb_audio_stop_triangle(sb_audio_t *audio);
bool sb_audio_triangle_active(sb_audio_t *audio);

#endif
