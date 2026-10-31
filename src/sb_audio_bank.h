/* Internal native triangle/noise/saw bank: no SDL, Lua, allocation, or device ownership. */
#ifndef SB_AUDIO_BANK_H
#define SB_AUDIO_BANK_H
#include "sb_audio_triangle.h"
#include "sb_audio_saw.h"
#define SB_AUDIO_BANK_VOICES 9u
#define SB_AUDIO_SYNTH_GAIN (1.0 / SB_AUDIO_BANK_VOICES)
typedef enum {
    SB_AUDIO_VOICE_TRIANGLE = 0, SB_AUDIO_VOICE_NOISE, SB_AUDIO_VOICE_SAW
} sb_audio_voice_kind_t;
typedef struct {
    sb_audio_triangle_t voices[SB_AUDIO_BANK_VOICES];
    sb_audio_noise_t noise[SB_AUDIO_BANK_VOICES];
    uint32_t seeds[SB_AUDIO_BANK_VOICES]; /* Zero selects the slot's fixed default. */
    sb_audio_saw_t saw[SB_AUDIO_BANK_VOICES];
    sb_audio_voice_kind_t kind[SB_AUDIO_BANK_VOICES];
} sb_audio_bank_t;

/* Zero initialization is valid and silent. Reset cancels all notes and restores
 * default configuration/controls; cleanup only, not an audible fade. */
void sb_audio_bank_reset(sb_audio_bank_t *bank);
/* Public native voice numbers are 1..9. Invalid requests return false without
 * mutation. Valid silent note-off/stop are harmless and return true. */
bool sb_audio_bank_configure(sb_audio_bank_t *bank, unsigned voice, const sb_envelope_settings_t *settings);
bool sb_audio_bank_note_on(sb_audio_bank_t *bank, unsigned voice, double frequency);
/* Noise has no pitch. Idle start resets to configured/default seed; active
 * noise retrigger preserves sequence. Active cross-type starts are rejected.
 * Seed edits affect the next idle noise start only; zero restores slot default. */
bool sb_audio_bank_noise_on(sb_audio_bank_t *bank, unsigned voice);
/* Saw starts at phase .5 when idle; active saw retrigger preserves phase.
 * Same 220..660 Hz range, ADSR/control semantics, and cross-type rejection. */
bool sb_audio_bank_saw_on(sb_audio_bank_t *bank, unsigned voice, double frequency);
bool sb_audio_bank_seed(sb_audio_bank_t *bank, unsigned voice, uint32_t seed);
bool sb_audio_bank_pitch(sb_audio_bank_t *bank, unsigned voice, double frequency);
bool sb_audio_bank_volume(sb_audio_bank_t *bank, unsigned voice, double volume);
bool sb_audio_bank_pan(sb_audio_bank_t *bank, unsigned voice, double pan);
bool sb_audio_bank_note_off(sb_audio_bank_t *bank, unsigned voice);
bool sb_audio_bank_stop(sb_audio_bank_t *bank, unsigned voice);
bool sb_audio_bank_active(const sb_audio_bank_t *bank, unsigned voice);
/* Replace output with interleaved stereo, summing all voices then multiplying
 * by fixed 1/9. No clamp, diagnostic attenuation, or active-count normalization.
 * Caller supplies room for 2*frames floats, not overlapping bank storage, and
 * owns synchronization. Zero frames permits NULL output. Invalid arguments
 * return false without advancing state. No allocations or unbounded scratch. */
bool sb_audio_bank_render(sb_audio_bank_t *bank, float *output, size_t frames);
#endif
