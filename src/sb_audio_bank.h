/* Internal native triangle bank: no SDL, Lua, allocation, or device ownership. */
#ifndef SB_AUDIO_BANK_H
#define SB_AUDIO_BANK_H
#include "sb_audio_triangle.h"
#define SB_AUDIO_BANK_VOICES 9u
#define SB_AUDIO_SYNTH_GAIN (1.0 / SB_AUDIO_BANK_VOICES)
typedef struct {
    sb_audio_triangle_t voices[SB_AUDIO_BANK_VOICES];
} sb_audio_bank_t;

/* Zero initialization is valid and silent. Reset cancels all notes and restores
 * default configuration/controls; cleanup only, not an audible fade. */
void sb_audio_bank_reset(sb_audio_bank_t *bank);
/* Public native voice numbers are 1..9. Invalid requests return false without
 * mutation. Valid silent note-off/stop are harmless and return true. */
bool sb_audio_bank_configure(sb_audio_bank_t *bank, unsigned voice, const sb_envelope_settings_t *settings);
bool sb_audio_bank_note_on(sb_audio_bank_t *bank, unsigned voice, double frequency);
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
