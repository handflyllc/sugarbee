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

/* Native prototype: 220--660 Hz, fixed attenuation 0.05. Configure validates all
 * four ADSR fields before updating; current notes retain their captured values.
 * Start/retrigger preserves active phase and attacks from current level over
 * the full new attack time; it refuses a playing diagnostic sine tone.
 * Pitch changes preserve envelope progress. Note-off uses captured release;
 * stop overrides it with 10 ms. Repeated note-off/stop do not restart fades.
 * Controls are host-thread only. NULL is safe/unavailable. Not a Lua API. */
bool sb_audio_configure_triangle(sb_audio_t *audio, double attack, double decay,
                                  double sustain, double release);
void sb_audio_note_off_triangle(sb_audio_t *audio);
bool sb_audio_start_triangle(sb_audio_t *audio, double frequency);
bool sb_audio_set_triangle_pitch(sb_audio_t *audio, double frequency);
void sb_audio_stop_triangle(sb_audio_t *audio);
bool sb_audio_triangle_active(sb_audio_t *audio);
/* Native volume (0..1, default .5) and equal-power pan (-1..1, default 0).
 * Finite inputs only. Active changes smooth over 10 ms; idle changes apply now.
 * Controls preserve phase/envelope and volume zero does not stop the voice. */
bool sb_audio_set_triangle_volume(sb_audio_t *audio, double volume);
bool sb_audio_set_triangle_pan(sb_audio_t *audio, double pan);

#endif
