/* Experimental prepared-song core. Internal interface, not a stable SDK. */
#ifndef SB_MUSIC_SEQUENCE_H
#define SB_MUSIC_SEQUENCE_H
#include "sb_audio_bank.h"

/* Prototype validation bound, not the future JSON asset budget. */
#define SB_MUSIC_SEQUENCE_MAX_EVENTS 4096u
#define SB_MUSIC_SEQUENCE_RATE 48000u
#define SB_MUSIC_GAIN_FRAMES 480u

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
    unsigned track; /* Logical 1..track_count; identity in private mode. */
    sb_music_event_kind_t kind;
    double hz; /* Zero for off/noise; 27.5..4186.01 for pitched on. */
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
    sb_audio_bank_t bank; /* Private player, or shared music/direct bank. */
    sb_audio_bank_t initial_bank; /* Prepared silent snapshot for looping. */
    bool loop;
    bool shared; /* Internal shared-player mode; survives music cleanup. */
    unsigned voice_map[SB_AUDIO_BANK_VOICES], reserved;
    double gain, gain_from, gain_target;
    unsigned gain_left;
    const sb_music_song_t *song;
    uint64_t frame;
    size_t next_event;
    sb_music_state_t state;
} sb_music_sequence_t;

/* Zero initialization is silent/idle. Caller owns all synchronization.
 * Reset is immediate cleanup, NOT an audible stop. Shared mode clears only
 * reserved slots and preserves the shared bank/mode for subsequent effects.
 * No allocation/IO/Lua/SDL. */
void sb_music_sequence_reset(sb_music_sequence_t *sequence);
/* Validate/setup outside the audio callback. Caller keeps song, instruments,
 * and events alive and immutable until idle/reset. Ordinary C pointer/array
 * validity is a caller obligation; this is not a parser for untrusted bytes.
 * Start refuses busy or invalid songs without changing sequence. Empty event
 * arrays/unused tracks are allowed.
 * pulse_width is required only for pulse (otherwise ignored).
 * Each successful start initializes gain to 1. No mapping, pause control,
 * or cartridge integration yet. */
bool sb_music_sequence_start(sb_music_sequence_t *sequence, const sb_music_song_t *song);
/* Validate and analyze a prepared song; false for invalid/ineligible songs.
 * Preparation only, never call from the callback. Used by the asset loader. */
bool sb_music_sequence_loop_eligible(const sb_music_song_t *song);
/* Explicit looping start, same lifetime/atomic-refusal contract as start.
 * Ineligible songs are refused. Snapshot is prepared before playback. Each
 * iteration restores it with no inserted samples. Remains PLAYING until reset;
 * stop fades to idle. Reset/release ownership only with callback excluded. */
bool sb_music_sequence_start_loop(sb_music_sequence_t *sequence, const sb_music_song_t *song);
/* Caller serializes with rendering (e.g. device lock). Cancel scheduling/loop
 * immediately; active voices use existing 480-frame stop fades. FINISHING until
 * render drains them, then silent defaults and song reference are cleared.
 * Repeated requests do not extend fades; idle succeeds, NULL returns false.
 * With no active voices cleanup is immediate. Natural release can be stopped.
 * Asset remains caller-owned, alive until idle/reset; no host-pause handling. */
bool sb_music_sequence_stop(sb_music_sequence_t *sequence);
/* Finite gain 0..1, playing/finishing only; false without mutation otherwise.
 * Caller serializes with render. Ramp from current gain over 480 frames; same
 * target does not restart ramp. Apply current gain before advancing each frame.
 * Mute does not stop time. Gain/ramp survives loop bank restoration; cleanup
 * clears it and the next start uses 1. No retained idle setting. */
bool sb_music_sequence_set_gain(sb_music_sequence_t *sequence, double gain);
/* Replace interleaved float stereo output. Buffer must hold 2*frames floats,
 * not alias sequence/song data. NULL output allowed only for zero frames.
 * Invalid buffer arguments do not advance state. All boundary events apply
 * before their sample; zero-frame renders do not dispatch events. End-frame
 * events are processed upon reaching that boundary even at a buffer end.
 * Natural tails finish before the bank resets and song reference is released. */
bool sb_music_sequence_render(sb_music_sequence_t *sequence, float *output, size_t frames);
/* Internal entry used by sb_music_shared only. Do not mix host-adapter or
 * private starts with shared playback. Map is copied; song remains borrowed. */
bool sb_music_sequence_start_mapped(sb_music_sequence_t *sequence,
    const sb_music_song_t *song, const unsigned *voices, size_t count, bool loop);
#endif
