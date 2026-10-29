/* Internal save storage API; no Lua binding yet. */
#ifndef SB_SAVE_H
#define SB_SAVE_H

#include <stddef.h>
#include <stdint.h>

#define SB_SAVE_MAX_BYTES (64u * 1024u)

typedef enum {
    SB_SAVE_OK = 0,
    SB_SAVE_NOT_FOUND,
    SB_SAVE_TOO_LARGE,
    SB_SAVE_IO_ERROR,
    SB_SAVE_INVALID_ARGUMENT,
    SB_SAVE_DURABILITY_UNCERTAIN
} sb_save_result_t;

/* Read <canonical UUID>.sav from a host-selected directory, without creating
 * files or directories. Only regular files are accepted; save-file symlinks
 * are rejected. A missing directory or save returns NOT_FOUND.
 * On success the caller owns *data (free it), even for an empty save.
 * Use *length, not strlen: bytes may contain zeros and are not NUL-terminated.
 * On failure, valid output pointers are reset to NULL/0. Invalid C arguments
 * return INVALID_ARGUMENT; host/Lua argument validation will be added later.
 */
sb_save_result_t sb_save_read(const char *directory, const uint8_t uuid[16],
                              uint8_t **data, size_t *length);

/* Write to an EXISTING host-selected directory. NULL data is valid only when
 * length is zero. Files are replaced, never edited in place. Existing save
 * entries must be regular files (no symlinks). No directory is created.
 * OK: file and directory synchronization succeeded.
 * IO_ERROR: replacement did not happen; the previous save is untouched.
 * DURABILITY_UNCERTAIN: replacement happened, but final directory sync/close
 * failed. The new save may already be present; no rollback is attempted.
 * Temporary-file removal is attempted on early failure; a cleanup error is
 * reported to stderr. Concurrent writers are outside the supported contract.
 */
sb_save_result_t sb_save_write(const char *directory, const uint8_t uuid[16],
                               const uint8_t *data, size_t length);

#endif
