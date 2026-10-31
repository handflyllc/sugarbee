/* Host-owned audio output. No Lua or asset API yet. */
#ifndef SB_AUDIO_H
#define SB_AUDIO_H
#include <stdbool.h>
#include <stdint.h>

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

/* Native bank diagnostic controls, voice numbers 1..9. The bank applies fixed
 * 1/9 headroom; this SDL diagnostic path additionally applies gain approximately 0.791.
 * Same pitch/ADSR/volume/pan rules as the triangle above. Independent slots,
 * no automatic allocation/stealing. Note-on refuses an active legacy diagnostic;
 * legacy diagnostics likewise refuse active bank voices. All calls lock the
 * device, are control-thread only, and return false for NULL/invalid requests.
 * Valid idle note-off/stop succeed harmlessly. No Lua API yet. */
bool sb_audio_voice_configure(sb_audio_t *audio, unsigned voice, double attack,
                              double decay, double sustain, double release);
bool sb_audio_voice_note_on(sb_audio_t *audio, unsigned voice, double frequency);
/* Noise-specific note-on and seed configuration. Zero seed restores the slot's
 * distinct fixed default. Idle note-on restarts the seed; active retrigger keeps
 * the sequence. Seed edits apply only on the next idle noise start. Triangle
 * and noise cannot replace one another while active; noise rejects pitch. */
/* Saw uses the same 220..660 Hz limits. Idle start resets phase to .5;
 * active saw retrigger/pitch changes preserve phase. Active type changes fail. */
bool sb_audio_voice_saw_on(sb_audio_t *audio, unsigned voice, double frequency);
bool sb_audio_voice_noise_on(sb_audio_t *audio, unsigned voice);
bool sb_audio_voice_seed(sb_audio_t *audio, unsigned voice, uint32_t seed);
bool sb_audio_voice_pitch(sb_audio_t *audio, unsigned voice, double frequency);
bool sb_audio_voice_volume(sb_audio_t *audio, unsigned voice, double volume);
bool sb_audio_voice_pan(sb_audio_t *audio, unsigned voice, double pan);
bool sb_audio_voice_note_off(sb_audio_t *audio, unsigned voice);
bool sb_audio_voice_stop(sb_audio_t *audio, unsigned voice);
bool sb_audio_voice_active(sb_audio_t *audio, unsigned voice);

#endif
