/* Bounded reads of named resources from SBEE .vcart archives (internal v0.1). */
#ifndef SB_ASSETS_H
#define SB_ASSETS_H

#include <stddef.h>
#include <stdint.h>

#define SB_ASSET_MAX_ARCHIVE (64u * 1024u * 1024u)
#define SB_ASSET_MAX_ENTRY (16u * 1024u * 1024u)
#define SB_ASSET_MAX_TOTAL (128u * 1024u * 1024u)
#define SB_ASSET_MAX_FILES 1024u

typedef enum {
    SB_ASSET_OK = 0,
    SB_ASSET_INVALID_PATH,
    SB_ASSET_INVALID_CARTRIDGE,
    SB_ASSET_NOT_FOUND,
    SB_ASSET_TOO_LARGE,
    SB_ASSET_CORRUPT,
    SB_ASSET_IO_ERROR
} sb_asset_result_t;

/*
 * Paths are relative ZIP member names with '/' separators, not OS paths.
 * Reads exactly one named entry, verifying ZIP CRC and enforcing resource caps.
 * Caller owns *data and releases it with free(). For empty files, *data is
 * still a non-NULL allocated pointer and *length is zero.
 */
sb_asset_result_t sb_asset_read(const char *cartridge, const char *path,
                                 uint8_t **data, size_t *length);
const char *sb_asset_error_string(sb_asset_result_t result);

#endif
