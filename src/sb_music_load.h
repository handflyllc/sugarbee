/* Internal, one-shot asset loader. No SDL/Lua or cartridge I/O. */
#ifndef SB_MUSIC_LOAD_H
#define SB_MUSIC_LOAD_H
#include "sb_music_sequence.h"
#define SB_MUSIC_LOAD_MAX_INPUT (1024u * 1024u)
#define SB_MUSIC_LOAD_MAX_PARSER (8u * 1024u * 1024u)
#define SB_MUSIC_LOAD_MAX_PREPARED (256u * 1024u)
#define SB_MUSIC_LOAD_MAX_DEPTH 5u
#define SB_MUSIC_LOAD_MAX_END UINT64_C(28800000)
typedef struct sb_music_asset sb_music_asset_t;
typedef enum {
    SB_MUSIC_LOAD_OK, SB_MUSIC_LOAD_ARGUMENT, SB_MUSIC_LOAD_SYNTAX,
    SB_MUSIC_LOAD_SCHEMA, SB_MUSIC_LOAD_LIMIT, SB_MUSIC_LOAD_MEMORY
} sb_music_load_code_t;
typedef struct {
    sb_music_load_code_t code;
    size_t byte_offset; /* SIZE_MAX unless syntax/depth location is available. */
    char message[192]; /* Bounded diagnostic, array indices are zero-based. */
    size_t parser_peak_bytes; /* Requested live bytes including allocator headers. */
    size_t prepared_bytes;
} sb_music_load_result_t;
/* Input need not be terminated; never modified. Return a new owned asset or NULL.
 * No playback state is touched. Result is optional, overwritten on every call.
 * All input/parser storage may be discarded on return. Call outside callback. */
sb_music_asset_t *sb_music_load(const char *json, size_t length,
                                sb_music_load_result_t *result);
const sb_music_song_t *sb_music_asset_song(const sb_music_asset_t *asset);
/* Cached preparation result: every final release ends at/before end_frame.
 * NULL returns false. Ineligible assets still load/play once. This is boundary
 * eligibility, not a promise of click-free looping. Pass the borrowed song to
 * sb_music_sequence_start_loop for native loop playback. */
bool sb_music_asset_loop_eligible(const sb_music_asset_t *asset);
/* NULL safe. Caller MUST ensure no sequencer still borrows this asset. No registry
 * or active-use tracking here; this is not the future Lua unload operation. */
void sb_music_asset_free(sb_music_asset_t *asset);
#endif
