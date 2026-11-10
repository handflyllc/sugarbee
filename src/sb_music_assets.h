/* Internal owned music collection, independent of SDL/Lua/cartridge IO. */
#ifndef SB_MUSIC_ASSETS_H
#define SB_MUSIC_ASSETS_H
#include "sb_music_load.h"
#define SB_MUSIC_ASSETS 8u
#define SB_MUSIC_ASSETS_MAX_BYTES (1024u * 1024u)
typedef uint64_t sb_music_handle_t;
typedef struct {
    unsigned track_count, timeline_rate;
    uint64_t end_frame;
    bool loop_eligible;
} sb_music_info_t;
/* Copy immutable metadata, never expose storage pointers to Lua. */
bool sb_music_asset_info(const sb_music_asset_t *asset,sb_music_info_t *info);
typedef enum {
    SB_MUSIC_ASSET_OK, SB_MUSIC_ASSET_ARGUMENT, SB_MUSIC_ASSET_UNAVAILABLE,
    SB_MUSIC_ASSET_REJECTED, SB_MUSIC_ASSET_LOAD_FAILED, SB_MUSIC_ASSET_CAPACITY,
    SB_MUSIC_ASSET_BAD_HANDLE, SB_MUSIC_ASSET_IN_USE, SB_MUSIC_ASSET_EXHAUSTED, SB_MUSIC_ASSET_READ_FAILED,
    SB_MUSIC_ASSET_PLAYER_BUSY, SB_MUSIC_ASSET_VOICE_BUSY, SB_MUSIC_ASSET_LOOP_INELIGIBLE
} sb_music_asset_code_t;
typedef struct { sb_music_handle_t id; sb_music_asset_t *asset; } sb_music_asset_slot_t;
typedef struct {
    sb_music_asset_slot_t slots[SB_MUSIC_ASSETS];
    size_t bytes;
    sb_music_handle_t serial;
} sb_music_assets_t;
/* Zero-init once. All access externally serialized. Handles belong to this
 * collection, not other instances. Reset preserves the serial across sessions;
 * exhaustion refuses new IDs instead of wrapping and reviving stale handles.
 * Insert transfers ownership only on success and clears *asset. The caller
 * retains ownership on refusal. Never insert an asset already owned elsewhere. */
sb_music_asset_code_t sb_music_assets_insert(sb_music_assets_t *pool,
    sb_music_asset_t **asset, sb_music_handle_t *handle);
const sb_music_asset_t *sb_music_assets_get(const sb_music_assets_t *pool,
    sb_music_handle_t handle);
/* Supply the owner's currently borrowed song, including paused/finishing state.
 * Only one player may borrow from a collection. Unload refuses that song.
 * Reset requires the player to have detached first. Both free on control thread;
 * neither is a render/callback operation. */
sb_music_asset_code_t sb_music_assets_unload(sb_music_assets_t *pool,
    sb_music_handle_t handle, const sb_music_song_t *borrowed);
void sb_music_assets_reset(sb_music_assets_t *pool);
#endif
