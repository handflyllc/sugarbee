/* Internal native triangle/noise/saw/pulse bank: no SDL, Lua, allocation, or device ownership. */
#ifndef SB_AUDIO_BANK_H
#define SB_AUDIO_BANK_H
#include "sb_audio_triangle.h"
#include "sb_audio_saw.h"
#include "sb_audio_pulse.h"
#include "sb_audio_filter.h"
#define SB_AUDIO_BANK_VOICES 9u
#define SB_AUDIO_SYNTH_GAIN (1.0 / SB_AUDIO_BANK_VOICES)
#include "sb_audio_waveform.h"
#define SB_AUDIO_SWITCH_FRAMES 480u
typedef struct {
    sb_audio_triangle_t voices[SB_AUDIO_BANK_VOICES];
    sb_audio_noise_t noise[SB_AUDIO_BANK_VOICES];
    uint32_t seeds[SB_AUDIO_BANK_VOICES]; /* Zero selects the slot's fixed default. */
    sb_audio_saw_t saw[SB_AUDIO_BANK_VOICES];
    sb_audio_pulse_t pulse[SB_AUDIO_BANK_VOICES];
    sb_audio_filter_t filters[SB_AUDIO_BANK_VOICES];
    double retained_pitch[SB_AUDIO_BANK_VOICES]; /* Zero selects default 440 Hz. */
    double widths[SB_AUDIO_BANK_VOICES]; /* Zero selects default .5. */
    sb_audio_voice_kind_t kind[SB_AUDIO_BANK_VOICES];
    sb_audio_voice_kind_t destination[SB_AUDIO_BANK_VOICES];
    unsigned switch_left[SB_AUDIO_BANK_VOICES];
} sb_audio_bank_t;

/* Zero initialization is valid and silent. Reset cancels all notes and restores
 * default configuration/controls; cleanup only, not an audible fade. */
void sb_audio_bank_reset(sb_audio_bank_t *bank);
/* Public native voice numbers are 1..9. Invalid requests return false without
 * mutation. Valid silent note-off/stop are harmless and return true. */
/* Select triangle/saw/pulse/noise without starting/retriggering a note. Idle selection
 * is immediate. Active switches crossfade for 480 frames; pitched-to-pitched
 * preserves phase. Entering noise restarts the configured/default seed; leaving
 * noise starts the destination at its normal phase and retained pitch (440 default).
 * Repeating the destination succeeds without restart; other busy requests fail.
 * Seed edits affect only the next fresh noise start, never the current stream. */
bool sb_audio_bank_waveform(sb_audio_bank_t *bank, unsigned voice, sb_audio_voice_kind_t kind);
/* Start selected pitched waveform (effective destination during a fade). Retriggers during a fade retain both phases and its
 * deadline; type-specific note-ons reject all starts during an active fade. */
bool sb_audio_bank_start_selected(sb_audio_bank_t *bank, unsigned voice, double frequency);
/* Noise-only selected start; no frequency argument. Active retrigger preserves
 * sequence/fade deadline. Rejects a selected pitched destination unchanged. */
bool sb_audio_bank_start_selected_noise(sb_audio_bank_t *bank, unsigned voice);
bool sb_audio_bank_configure(sb_audio_bank_t *bank, unsigned voice, const sb_envelope_settings_t *settings);
bool sb_audio_bank_note_on(sb_audio_bank_t *bank, unsigned voice, double frequency);
/* Noise has no pitch. Idle start resets to configured/default seed; active
 * noise retrigger preserves sequence. Active cross-type starts are rejected.
 * Seed edits affect the next fresh noise start/transition; zero restores slot default. */
bool sb_audio_bank_noise_on(sb_audio_bank_t *bank, unsigned voice);
/* Saw starts at phase .5 when idle; active saw retrigger preserves phase.
 * Same 110..880 Hz range, ADSR/control semantics, and cross-type rejection. */
bool sb_audio_bank_saw_on(sb_audio_bank_t *bank, unsigned voice, double frequency);
/* Conditioned pulse: idle starts at phase zero with stored width (default .5).
 * Active retrigger preserves phase and width ramp. Pitch range 110..880 Hz.
 * Width 0.05..0.95: active pulse ramps 10 ms; otherwise store for next pulse start.
 * Width persists across other waveform selections; reset restores default. */
bool sb_audio_bank_pulse_on(sb_audio_bank_t *bank, unsigned voice, double frequency);
bool sb_audio_bank_pulse_width(sb_audio_bank_t *bank, unsigned voice, double width);
bool sb_audio_bank_seed(sb_audio_bank_t *bank, unsigned voice, uint32_t seed);
/* Active voices only, finite 110..880 Hz. Updates retained pitch and any audible
 * pitched source; during steady noise only the retained frequency changes. */
bool sb_audio_bank_pitch(sb_audio_bank_t *bank, unsigned voice, double frequency);
bool sb_audio_bank_volume(sb_audio_bank_t *bank, unsigned voice, double volume);
bool sb_audio_bank_pan(sb_audio_bank_t *bank, unsigned voice, double pan);
/* One-pole low-pass after source blending, before ADSR/volume/pan.
 * Cutoff 20..20000 Hz (default 2000), bypass true by default. Cutoff edits do
 * not enable filtering. Live coefficient and dry/wet changes ramp 480 frames;
 * idle edits are immediate. Same targets preserve deadlines. Active retrigger
 * and waveform changes retain history; fresh notes clear it and settle ramps.
 * Muted/bypassed active voices still advance history. Reset restores defaults. */
bool sb_audio_bank_filter_cutoff(sb_audio_bank_t *bank, unsigned voice, double cutoff);
bool sb_audio_bank_filter_bypass(sb_audio_bank_t *bank, unsigned voice, bool bypass);
bool sb_audio_bank_note_off(sb_audio_bank_t *bank, unsigned voice);
bool sb_audio_bank_stop(sb_audio_bank_t *bank, unsigned voice);
bool sb_audio_bank_active(const sb_audio_bank_t *bank, unsigned voice);
/* Replace output with interleaved stereo, summing all voices then multiplying
 * by fixed 1/9. No clamp, diagnostic attenuation, or active-count normalization.
 * Caller supplies room for 2*frames floats, not overlapping bank storage, and
 * owns synchronization. Zero frames permits NULL output. Invalid arguments
 * return false without advancing state. No allocations or unbounded scratch. */
bool sb_audio_bank_render(sb_audio_bank_t *bank, float *output, size_t frames);
/* Internal slot operations include oscillator, filter, transition and retained
 * settings. No ownership policy here; shared-player controls enforce that. */
bool sb_audio_bank_copy_voice(sb_audio_bank_t *dst, unsigned to,
                              const sb_audio_bank_t *src, unsigned from);
/* Render every physical slot exactly once. Gain affects only mask slots;
 * both groups retain fixed 1/9 headroom. Invalid masks/gain refuse unchanged. */
bool sb_audio_bank_render_music(sb_audio_bank_t *bank, unsigned mask, double gain,
                                float *output, size_t frames);
#endif
