/* Host-owned audio output. No Lua, synthesis, or asset API yet. */
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

#endif
