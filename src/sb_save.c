#define _POSIX_C_SOURCE 200809L
#include "sb_save.h"
#include "vcart.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

sb_save_result_t sb_save_read(const char *directory, const uint8_t uuid[16],
                              uint8_t **data, size_t *length) {
    if (data) *data = NULL;
    if (length) *length = 0;
    if (!directory || !*directory || !uuid || !data || !length)
        return SB_SAVE_INVALID_ARGUMENT;

    int dirfd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd < 0)
        return errno == ENOENT ? SB_SAVE_NOT_FOUND : SB_SAVE_IO_ERROR;

    char filename[41];
    vcart_uuid_to_string(uuid, filename);
    memcpy(filename + 36, ".sav", 5);
    /* NONBLOCK prevents a misplaced FIFO from waiting for a writer. */
    int fd = openat(dirfd, filename, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    int open_error = errno;
    int dir_close = close(dirfd);
    if (fd < 0)
        return dir_close == 0 && open_error == ENOENT
                   ? SB_SAVE_NOT_FOUND : SB_SAVE_IO_ERROR;

    sb_save_result_t result = SB_SAVE_IO_ERROR;
    uint8_t *buffer = NULL;
    size_t used = 0;
    struct stat info;
    if (dir_close != 0 || fstat(fd, &info) != 0 || !S_ISREG(info.st_mode))
        goto cleanup;
    if (info.st_size > (off_t)SB_SAVE_MAX_BYTES) {
        result = SB_SAVE_TOO_LARGE;
        goto cleanup;
    }

    /* Read one extra byte to detect growth after fstat, with bounded allocation. */
    buffer = malloc(SB_SAVE_MAX_BYTES + 1u);
    if (!buffer) goto cleanup;
    while (used < SB_SAVE_MAX_BYTES + 1u) {
        ssize_t n = read(fd, buffer + used, SB_SAVE_MAX_BYTES + 1u - used);
        if (n < 0) {
            if (errno == EINTR) continue;
            goto cleanup;
        }
        if (n == 0) break;
        used += (size_t)n;
    }
    result = used > SB_SAVE_MAX_BYTES ? SB_SAVE_TOO_LARGE : SB_SAVE_OK;

cleanup:
    if (close(fd) != 0) result = SB_SAVE_IO_ERROR;
    if (result == SB_SAVE_OK) {
        *data = buffer;
        *length = used;
    } else {
        free(buffer);
    }
    return result;
}
