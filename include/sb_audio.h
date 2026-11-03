/* Host-owned audio output. No Lua or asset API yet. */
#ifndef SB_AUDIO_H
#define SB_AUDIO_H
#include <stdbool.h>
#include <stdint.h>
#include "sb_audio_waveform.h"

typedef struct sb_audio sb_audio_t;

/* Open default output and start supplying silence. Returns NULL and emits one
 * diagnostic on failure; the host may continue without audio. Call once during
 * host startup, not per cartridge. SDL audio initialization is owned here. */
sb_audio_t *sb_audio_create(void);

/* Close output before releasing callback resources and the SDL audio reference.
 * NULL is harmless. Call after cartridge shutdown and before SDL_Quit(). */
void sb_audio_destroy(sb_audio_t *audio);

/* Host cartridge lifecycle; NULL is harmless. Begin resets all voices/settings
 * and opens a fresh session. Pause fades for 480 frames then freezes the entire
 * bank; resume fades in from the frozen state. Repeated pause requests are no-ops;
 * reversals ramp from current gain. Fail rejects bank controls immediately,
 * fades out, then resets voices; it cannot be resumed. End immediately clears
 * voices/settings and closes the session without waiting for a fade/device.
 * Eligible shutdown runs before End; a paused shutdown remains silent.
 * Host control thread only. Fresh devices allow diagnostics until managed;
 * between sessions bank controls are rejected. No Lua API in this checkpoint. */
void sb_audio_session_begin(sb_audio_t *audio);
void sb_audio_session_pause(sb_audio_t *audio, bool paused);
void sb_audio_session_fail(sb_audio_t *audio);
void sb_audio_session_end(sb_audio_t *audio);
/* Before eligible shutdown: settle a pending pause fade to silence, preserving
 * command availability and state until End. Running sessions remain audible. */
void sb_audio_session_prepare_shutdown(sb_audio_t *audio);

/* Native diagnostic only: request one quiet, one-second 440 Hz tone.
 * False if unavailable or already active (does not restart). Call on the host
 * control thread, never in the audio callback. Normal host never requests it. */
bool sb_audio_start_test_tone(sb_audio_t *audio);
bool sb_audio_test_tone_active(sb_audio_t *audio);

/* Native prototype: 110--880 Hz, fixed attenuation 0.05. Configure validates all
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
/* Triangle/saw/pulse/noise selection, separate from note-on. Active selection fades
 * 10 ms without restarting envelope; same destination is a no-op, other busy
 * requests fail. Idle selection stays silent. Entering noise starts a fresh seed;
 * leaving noise uses retained pitch (default 440) and normal destination phase. */
bool sb_audio_voice_waveform(sb_audio_t *audio, unsigned voice, sb_audio_voice_kind_t kind);
/* Start/retrigger selected pitched waveform, preserving any active fade. */
bool sb_audio_voice_start_selected(sb_audio_t *audio, unsigned voice, double frequency);
/* Start/retrigger selected noise without a frequency; preserves active sequence
 * and fade deadline. The effective destination must be noise. */
bool sb_audio_voice_start_selected_noise(sb_audio_t *audio, unsigned voice);
bool sb_audio_voice_configure(sb_audio_t *audio, unsigned voice, double attack,
                              double decay, double sustain, double release);
bool sb_audio_voice_note_on(sb_audio_t *audio, unsigned voice, double frequency);
/* Noise-specific note-on and seed configuration. Zero seed restores the slot's
 * distinct fixed default. Idle note-on restarts the seed; active retrigger keeps
 * the sequence. Seed edits apply on the next fresh noise start/transition.
 * Type-specific note-ons reject active cross-type starts and all active fades. */
/* Saw uses the same 110..880 Hz limits. Idle start resets phase to .5;
 * active saw retrigger/pitch changes preserve phase. Active type changes fail. */
bool sb_audio_voice_saw_on(sb_audio_t *audio, unsigned voice, double frequency);
/* DC-conditioned pulse, 110..880 Hz. Idle start resets phase to zero and uses
 * stored width (default .5); active retrigger preserves phase/width ramp.
 * Width 0.05..0.95 ramps 10 ms on active pulse; otherwise stored for next start. */
bool sb_audio_voice_pulse_on(sb_audio_t *audio, unsigned voice, double frequency);
bool sb_audio_voice_pulse_width(sb_audio_t *audio, unsigned voice, double width);
bool sb_audio_voice_noise_on(sb_audio_t *audio, unsigned voice);
bool sb_audio_voice_seed(sb_audio_t *audio, unsigned voice, uint32_t seed);
/* Active voices: finite 110..880 Hz. On steady noise updates retained pitch
 * without changing its sound; during fades updates any audible pitched source. */
bool sb_audio_voice_pitch(sb_audio_t *audio, unsigned voice, double frequency);
bool sb_audio_voice_volume(sb_audio_t *audio, unsigned voice, double volume);
bool sb_audio_voice_pan(sb_audio_t *audio, unsigned voice, double pan);
/* Per-voice non-resonant low-pass after waveform blending, before ADSR/pan.
 * Cutoff finite 20..20000 Hz, default 2000; bypass true by default. Setting
 * cutoff does not enable the filter. Live coefficient and bypass changes ramp
 * for 10 ms, idle edits apply immediately; repeated targets keep deadlines.
 * Fresh notes clear history and settle retained targets. Active retriggers and
 * waveform switches preserve it. Muting/bypass do not freeze active history.
 * Bank controls only; legacy standalone diagnostics remain unfiltered. */
bool sb_audio_voice_filter_cutoff(sb_audio_t *audio, unsigned voice, double cutoff);
bool sb_audio_voice_filter_bypass(sb_audio_t *audio, unsigned voice, bool bypass);
bool sb_audio_voice_note_off(sb_audio_t *audio, unsigned voice);
bool sb_audio_voice_stop(sb_audio_t *audio, unsigned voice);
bool sb_audio_voice_active(sb_audio_t *audio, unsigned voice);

#endif
