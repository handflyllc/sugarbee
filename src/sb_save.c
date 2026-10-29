#define _POSIX_C_SOURCE 200809L
#include "sb_save.h"
#include "vcart.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
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

/* Synchronization may be interrupted without completing. Retry that case only. */
static int sync_save_fd(int fd) {
    int result;
    do {
        result = fsync(fd);
    } while (result < 0 && errno == EINTR);
    return result;
}

sb_save_result_t sb_save_write(const char *directory, const uint8_t uuid[16],
                               const uint8_t *data, size_t length) {
    if (!directory || !*directory || !uuid || (!data && length != 0))
        return SB_SAVE_INVALID_ARGUMENT;
    if (length > SB_SAVE_MAX_BYTES) return SB_SAVE_TOO_LARGE;

    int dirfd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd < 0) return SB_SAVE_IO_ERROR;

    sb_save_result_t result = SB_SAVE_IO_ERROR;
    char filename[41], temporary[96];
    vcart_uuid_to_string(uuid, filename);
    memcpy(filename + 36, ".sav", 5);
    int fd = -1;
    bool temporary_exists = false;
    struct stat info;
    if (fstatat(dirfd, filename, &info, AT_SYMLINK_NOFOLLOW) == 0) {
        if (!S_ISREG(info.st_mode)) goto cleanup;
    } else if (errno != ENOENT) {
        goto cleanup;
    }

    /* Exclusive creation never overwrites an existing temporary file. Names
       are private implementation details, not candidate save filenames.
       Bounded retries tolerate leftovers without an unbounded collision loop. */
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        snprintf(temporary, sizeof(temporary), ".%s.tmp.%ld.%u",
                 filename, (long)getpid(), attempt);
        fd = openat(dirfd, temporary,
                    O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd >= 0) {
            temporary_exists = true;
            break;
        }
        if (errno != EEXIST) goto cleanup;
    }
    if (fd < 0) goto cleanup;

    size_t written = 0;
    while (written < length) {
        ssize_t n = write(fd, data + written, length - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            goto cleanup;
        }
        if (n == 0) goto cleanup; /* Do not spin on a write making no progress. */
        written += (size_t)n;
    }
    if (sync_save_fd(fd) != 0) goto cleanup;
    /* On Linux a failed close can already have released the descriptor.
       Never retry close: it could act on a reused descriptor. */
    int close_result = close(fd);
    fd = -1;
    if (close_result != 0) goto cleanup;

    if (renameat(dirfd, temporary, dirfd, filename) != 0) goto cleanup;
    temporary_exists = false;
    /* Past this point IO_ERROR would falsely imply the old save is intact. */
    result = sync_save_fd(dirfd) == 0 ? SB_SAVE_OK : SB_SAVE_DURABILITY_UNCERTAIN;

cleanup:
    if (fd >= 0) close(fd);
    if (temporary_exists && unlinkat(dirfd, temporary, 0) != 0) {
        fprintf(stderr, "[SugarBee] Could not remove temporary save '%s': %s\n",
                temporary, strerror(errno));
    }
    if (close(dirfd) != 0 && result == SB_SAVE_OK)
        result = SB_SAVE_DURABILITY_UNCERTAIN;
    return result;
}

sb_save_result_t sb_save_prepare_directory(const char *parent, const char *child) {
    if (!parent || !*parent || !child || !*child || strchr(child, '/') ||
        strcmp(child, ".") == 0 || strcmp(child, "..") == 0)
        return SB_SAVE_INVALID_ARGUMENT;

    int parentfd = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (parentfd < 0) return SB_SAVE_IO_ERROR;

    sb_save_result_t result = SB_SAVE_IO_ERROR;
    int childfd = -1;
    if (mkdirat(parentfd, child, 0700) != 0 && errno != EEXIST) goto cleanup;

    /* EEXIST alone does not establish that this is a directory. Do not follow
       a child symlink, even one that points at a directory. */
    childfd = openat(parentfd, child, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (childfd < 0) goto cleanup;

    /* Repeat both syncs for existing children: a previous attempt may have
       created the directory but failed before its parent entry was durable. */
    if (sync_save_fd(childfd) != 0 || sync_save_fd(parentfd) != 0) goto cleanup;
    result = SB_SAVE_OK;

cleanup:
    if (childfd >= 0 && close(childfd) != 0) result = SB_SAVE_IO_ERROR;
    if (close(parentfd) != 0) result = SB_SAVE_IO_ERROR;
    /* Never remove the child on failure; callers may retry and it may contain
       existing data. No save replacement has occurred in this operation. */
    return result;
}
