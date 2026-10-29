/* Internal save storage API; no Lua binding yet. */
#ifndef SB_SAVE_H
#define SB_SAVE_H

#include <stddef.h>
#include <stdint.h>

#define SB_SAVE_MAX_BYTES (64u * 1024u)
/* Limits for a relative nested path, excluding its terminating NUL. */
#define SB_SAVE_MAX_PATH_BYTES 1024u
#define SB_SAVE_MAX_COMPONENTS 32u
#define SB_SAVE_MAX_COMPONENT_BYTES 255u

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

/* Prepare exactly one child beneath an existing host-selected parent.
 * child must be a single nonempty name, not ".", "..", or contain '/'.
 * Creates with mode 0700 subject to umask; existing permissions are unchanged.
 * The child must be a real directory, not a symlink. The parent path may
 * resolve through symlinks and its own placement must already be durable.
 * Both new and existing children are synchronized, then their parent is
 * synchronized. All descriptors are closed before success is reported.
 * IO_ERROR may leave a created directory in place; retry preparation before
 * relying on it. Save contents are never touched. Concurrent directory changes
 * are unsupported. This operation does not invoke the writer.
 */
sb_save_result_t sb_save_prepare_directory(const char *parent, const char *child);

/* Prepare a nonempty relative path beneath an existing, durable base.
 * Validate ALL components before creating anything: no absolute paths, empty
 * components (including trailing '/'), '.', '..', or components over the
 * limits above. Reject symlinks at every relative component. Existing
 * components are synchronized too. IO_ERROR may leave directories for retry.
 * The base itself may resolve through symlinks; its ancestors are not prepared.
 */
sb_save_result_t sb_save_prepare_path(const char *base, const char *relative);

/* Validate data/size/path, prepare the nested directory, then replace the save
 * using the same opened leaf directory (no path re-resolution). Preparation
 * failures return IO_ERROR and never invoke the writer. Once writing starts,
 * the result contract is the same as sb_save_write. No host path selection,
 * migration, environment-variable lookup, or Lua integration is performed.
 */
sb_save_result_t sb_save_write_prepared(const char *base, const char *relative,
                                       const uint8_t uuid[16],
                                       const uint8_t *data, size_t length);

#endif
