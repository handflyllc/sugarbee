/* Internal prepared-song SDL controls; no cartridge loading or Lua API. */
#ifndef SB_AUDIO_MUSIC_H
#define SB_AUDIO_MUSIC_H
#include "sb_audio.h"
#include "sb_music_sequence.h"
#include "sb_music_assets.h"
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
/* Owned assets: session mode only, one serialized control thread per device.
 * Load parses outside the device lock/callback, publishes atomically under lock.
 * On failure *handle is zero; detail describes parsing only (if attempted).
 * Handles are scoped to this device; unload/session teardown invalidates them.
 * Stop/reset releases playback's borrow, not the loaded asset. Failure retains
 * asset memory until begin/end/destroy; no callback allocations or frees.
 * Borrowed-song start above and handle play below share the same single player. */
sb_music_asset_code_t sb_audio_music_load(sb_audio_t *audio, const char *json,
    size_t length, sb_music_handle_t *handle, sb_music_load_result_t *detail);
sb_music_asset_code_t sb_audio_music_unload(sb_audio_t *audio, sb_music_handle_t handle);
sb_music_asset_code_t sb_audio_music_play(sb_audio_t *audio, sb_music_handle_t handle,
    const unsigned *voices, size_t count, bool loop);
#endif
