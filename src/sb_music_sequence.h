/* Experimental prepared-song core. Internal interface, not a stable SDK. */
#ifndef SB_MUSIC_SEQUENCE_H
#define SB_MUSIC_SEQUENCE_H
#include "sb_audio_bank.h"

/* Prototype validation bound, not the future JSON asset budget. */
#define SB_MUSIC_SEQUENCE_MAX_EVENTS 4096u
#define SB_MUSIC_SEQUENCE_RATE 48000u

typedef struct {
    sb_audio_voice_kind_t waveform;
    sb_envelope_settings_t envelope;
    double volume, pan, pulse_width, filter_cutoff;
    bool filter_bypass;
} sb_music_instrument_t;

/* Enum order is also the canonical same-frame ordering. */
typedef enum { SB_MUSIC_NOTE_OFF = 0, SB_MUSIC_NOTE_ON = 1 } sb_music_event_kind_t;
typedef struct {
    uint64_t frame;
    unsigned track; /* 1..track_count; maps to the same private bank slot. */
    sb_music_event_kind_t kind;
    double hz; /* Zero for off/noise; 110..880 for pitched on. */
} sb_music_event_t;
typedef struct {
    unsigned timeline_rate, track_count;
    uint64_t end_frame;
    const sb_music_instrument_t *instruments;
    const sb_music_event_t *events;
    size_t event_count;
} sb_music_song_t;

typedef enum { SB_MUSIC_IDLE = 0, SB_MUSIC_PLAYING, SB_MUSIC_FINISHING } sb_music_state_t;
typedef struct {
    sb_audio_bank_t bank; /* Exclusive private bank for this checkpoint. */
    sb_audio_bank_t initial_bank; /* Prepared silent snapshot for looping. */
    bool loop;
    const sb_music_song_t *song;
    uint64_t frame;
    size_t next_event;
    sb_music_state_t state;
} sb_music_sequence_t;

/* Zero initialization is silent/idle. Caller owns all synchronization.
 * Reset is immediate cleanup, NOT an audible stop. No allocation/IO/Lua/SDL. */
void sb_music_sequence_reset(sb_music_sequence_t *sequence);
/* Validate/setup outside the audio callback. Caller keeps song, instruments,
 * and events alive and immutable until idle/reset. Ordinary C pointer/array
 * validity is a caller obligation; this is not a parser for untrusted bytes.
 * Start refuses busy or invalid songs without changing sequence. Empty event
 * arrays/unused tracks are allowed.
 * pulse_width is required only for pulse (otherwise ignored).
 * No gain API, mapping, pause control, or cartridge integration yet. */
bool sb_music_sequence_start(sb_music_sequence_t *sequence, const sb_music_song_t *song);
/* Validate and analyze a prepared song; false for invalid/ineligible songs.
 * Preparation only, never call from the callback. Used by the asset loader. */
bool sb_music_sequence_loop_eligible(const sb_music_song_t *song);
/* Explicit looping start, same lifetime/atomic-refusal contract as start.
 * Ineligible songs are refused. Snapshot is prepared before playback. Each
 * iteration restores it with no inserted samples. Remains PLAYING until reset;
 * no audible stop API yet. Reset/release ownership only with callback excluded. */
bool sb_music_sequence_start_loop(sb_music_sequence_t *sequence, const sb_music_song_t *song);
/* Replace interleaved float stereo output. Buffer must hold 2*frames floats,
 * not alias sequence/song data. NULL output allowed only for zero frames.
 * Invalid buffer arguments do not advance state. All boundary events apply
 * before their sample; zero-frame renders do not dispatch events. End-frame
 * events are processed upon reaching that boundary even at a buffer end.
 * Natural tails finish before the bank resets and song reference is released. */
bool sb_music_sequence_render(sb_music_sequence_t *sequence, float *output, size_t frames);
#endif
