/* Internal prepared-song SDL controls; no cartridge loading or Lua API. */
#ifndef SB_AUDIO_MUSIC_H
#define SB_AUDIO_MUSIC_H
#include "sb_audio.h"
#include "sb_music_sequence.h"
/* Session mode only. Device locking serializes all controls with rendering.
 * Start validates atomically, copies the map, and borrows the immutable song
 * and its arrays until idle/reset/session end/destroy. Stop is a fade, not an
 * immediate release of that borrow. Reset is immediate and music-only.
 * Queries remain available after failure so owners can observe borrow release.
 * No voice stealing: mapped voices must be idle. */
bool sb_audio_music_start(sb_audio_t *audio, const sb_music_song_t *song,
    const unsigned *voices, size_t count, bool loop);
bool sb_audio_music_stop(sb_audio_t *audio);
bool sb_audio_music_reset(sb_audio_t *audio);
bool sb_audio_music_gain(sb_audio_t *audio, double gain);
sb_music_state_t sb_audio_music_state(sb_audio_t *audio);
bool sb_audio_music_reserved(sb_audio_t *audio, unsigned voice);
#endif
