#define _POSIX_C_SOURCE 200809L
#include "sb_save.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Link-time wrappers affect this test executable only. All uninjected calls
   use real filesystem operations; production code has no fault-control API. */
ssize_t __real_write(int fd, const void *data, size_t size);
int __real_fsync(int fd);
int __real_close(int fd);
int __real_renameat(int oldfd, const char *oldname, int newfd, const char *newname);
int __real_unlinkat(int fd, const char *name, int flags);

typedef enum {
    NONE, SHORT_WRITE, INTERRUPTED_WRITE, ZERO_WRITE, WRITE_FAILURE,
    PARTIAL_FAILURE, FILE_SYNC_FAILURE, FILE_CLOSE_FAILURE, RENAME_FAILURE,
    DIRECTORY_SYNC_FAILURE, DIRECTORY_CLOSE_FAILURE, CLEANUP_FAILURE,
    INTERRUPTED_SYNC
} fault_t;
static fault_t fault;
static unsigned writes, injections;
static bool file_synced, file_closed, replaced, directory_synced;
static bool order_error;

static bool is_directory(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0 && S_ISDIR(st.st_mode);
}

ssize_t __wrap_write(int fd, const void *data, size_t size) {
    ++writes;
    if (fault == INTERRUPTED_WRITE && writes == 1) {
        ++injections; errno = EINTR; return -1;
    }
    if (fault == ZERO_WRITE) { ++injections; return 0; }
    if (fault == WRITE_FAILURE || fault == CLEANUP_FAILURE ||
        (fault == PARTIAL_FAILURE && writes > 1)) {
        ++injections; errno = ENOSPC; return -1;
    }
    if ((fault == SHORT_WRITE || fault == PARTIAL_FAILURE) && size > 3) {
        ++injections; size = 3;
    }
    return __real_write(fd, data, size);
}

int __wrap_fsync(int fd) {
    bool directory = is_directory(fd);
    if (directory && !replaced) order_error = true;
    if (fault == INTERRUPTED_SYNC && injections == 0) {
        ++injections; errno = EINTR; return -1;
    }
    if ((fault == FILE_SYNC_FAILURE && !directory) ||
        (fault == DIRECTORY_SYNC_FAILURE && directory)) {
        ++injections; errno = EIO; return -1;
    }
    int result = __real_fsync(fd);
    if (result == 0) {
        if (directory) directory_synced = true;
        else file_synced = true;
    }
    return result;
}

int __wrap_close(int fd) {
    bool directory = is_directory(fd);
    int result = __real_close(fd);
    if (result == 0 && !directory) file_closed = true;
    if ((fault == FILE_CLOSE_FAILURE && !directory) ||
        (fault == DIRECTORY_CLOSE_FAILURE && directory)) {
        ++injections; errno = EIO; return -1;
    }
    return result;
}

int __wrap_renameat(int oldfd, const char *oldname, int newfd, const char *newname) {
    if (!file_synced || !file_closed || oldfd != newfd) order_error = true;
    if (fault == RENAME_FAILURE) {
        ++injections; errno = EIO; return -1;
    }
    int result = __real_renameat(oldfd, oldname, newfd, newname);
    if (result == 0) replaced = true;
    return result;
}

int __wrap_unlinkat(int fd, const char *name, int flags) {
    if (fault == CLEANUP_FAILURE) {
        ++injections; errno = EACCES; return -1;
    }
    return __real_unlinkat(fd, name, flags);
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Save writer check failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static const uint8_t uuid[16] = {
    0x12,0x3e,0x45,0x67,0xe8,0x9b,0x42,0xd3,
    0xa4,0x56,0x42,0x66,0x14,0x17,0x40,0x00
};

static sb_save_result_t save(const char *root, const uint8_t *id,
                             const uint8_t *data, size_t size, fault_t requested) {
    fault = requested;
    writes = injections = 0;
    file_synced = file_closed = replaced = directory_synced = order_error = false;
    sb_save_result_t result = sb_save_write(root, id, data, size);
    fault = NONE; /* Reader and fixture cleanup must not be fault injected. */
    return result;
}

static bool matches(const char *root, const uint8_t *id,
                    const uint8_t *expected, size_t size) {
    uint8_t *data = NULL;
    size_t length = 0;
    sb_save_result_t result = sb_save_read(root, id, &data, &length);
    bool ok = result == SB_SAVE_OK && length == size &&
              (size == 0 || memcmp(data, expected, size) == 0);
    free(data);
    return ok;
}

/* Count leftover temporary files, optionally remove only those test entries. */
static int temporaries(const char *root, bool remove_files) {
    DIR *dir = opendir(root);
    if (!dir) return -1;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, ".tmp.")) {
            ++count;
            if (remove_files && unlinkat(dirfd(dir), entry->d_name, 0) != 0) {
                closedir(dir); return -1;
            }
        }
    }
    if (closedir(dir) != 0) return -1;
    return count;
}

int main(void) {
    char root[] = "/tmp/sugarbee-save-writer-XXXXXX";
    CHECK(mkdtemp(root));
    char path[256], otherpath[256], missing[256];
    snprintf(path, sizeof(path), "%s/123e4567-e89b-42d3-a456-426614174000.sav", root);
    snprintf(otherpath, sizeof(otherpath), "%s/123e4567-e89b-42d3-a456-426614174001.sav", root);
    snprintf(missing, sizeof(missing), "%s/absent", root);
    uint8_t bytes[SB_SAVE_MAX_BYTES + 1u];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)(i % 256);
    const size_t sizes[] = {0, 1, 513, SB_SAVE_MAX_BYTES, 0};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        CHECK(save(root, uuid, sizes[i] ? bytes : NULL, sizes[i], NONE) == SB_SAVE_OK);
        CHECK(replaced && file_synced && file_closed && directory_synced && !order_error);
        CHECK(matches(root, uuid, bytes, sizes[i]));
        CHECK(temporaries(root, false) == 0);
    }
    const uint8_t old[] = "previous save";
    uint8_t other[16]; memcpy(other, uuid, 16); ++other[15];
    CHECK(save(root, other, old, sizeof(old), NONE) == SB_SAVE_OK);
    CHECK(save(root, uuid, bytes, 513, NONE) == SB_SAVE_OK);
    CHECK(matches(root, other, old, sizeof(old)));

    CHECK(save(root, uuid, bytes, sizeof(bytes), NONE) == SB_SAVE_TOO_LARGE);
    CHECK(!replaced && writes == 0 && matches(root, uuid, bytes, 513));
    CHECK(save(root, uuid, NULL, 1, NONE) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(save(root, NULL, bytes, 1, NONE) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(save(NULL, uuid, bytes, 1, NONE) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(save("", uuid, bytes, 1, NONE) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(save(missing, uuid, bytes, 1, NONE) == SB_SAVE_IO_ERROR);
    CHECK(access(missing, F_OK) != 0);
    CHECK(save(path, uuid, bytes, 1, NONE) == SB_SAVE_IO_ERROR);

    const fault_t faults[] = {
        WRITE_FAILURE, PARTIAL_FAILURE, ZERO_WRITE, FILE_SYNC_FAILURE,
        FILE_CLOSE_FAILURE, RENAME_FAILURE, DIRECTORY_SYNC_FAILURE,
        DIRECTORY_CLOSE_FAILURE, CLEANUP_FAILURE
    };
    for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
        CHECK(save(root, uuid, old, sizeof(old), NONE) == SB_SAVE_OK);
        bool uncertain = faults[i] == DIRECTORY_SYNC_FAILURE || faults[i] == DIRECTORY_CLOSE_FAILURE;
        CHECK(save(root, uuid, bytes, 513, faults[i]) ==
              (uncertain ? SB_SAVE_DURABILITY_UNCERTAIN : SB_SAVE_IO_ERROR));
        CHECK(injections > 0 && !order_error && replaced == uncertain);
        CHECK(matches(root, uuid, uncertain ? bytes : old, uncertain ? 513 : sizeof(old)));
        CHECK(matches(root, other, old, sizeof(old)));
        CHECK(temporaries(root, false) == (faults[i] == CLEANUP_FAILURE ? 1 : 0));
        CHECK(temporaries(root, true) >= 0);
    }
    const fault_t retries[] = {SHORT_WRITE, INTERRUPTED_WRITE, INTERRUPTED_SYNC};
    for (size_t i = 0; i < sizeof(retries) / sizeof(retries[0]); ++i) {
        CHECK(save(root, uuid, bytes, 513, retries[i]) == SB_SAVE_OK);
        CHECK(injections > 0 && directory_synced && !order_error);
        CHECK(matches(root, uuid, bytes, 513) && temporaries(root, false) == 0);
    }

    /* A failed first save must not create a UUID file. */
    CHECK(unlink(path) == 0);
    CHECK(save(root, uuid, bytes, 513, PARTIAL_FAILURE) == SB_SAVE_IO_ERROR);
    uint8_t *data = NULL; size_t length = 0;
    CHECK(sb_save_read(root, uuid, &data, &length) == SB_SAVE_NOT_FOUND);
    CHECK(temporaries(root, false) == 0);

    /* Refuse non-regular destinations without replacing them. */
    CHECK(mkdir(path, 0700) == 0);
    CHECK(save(root, uuid, bytes, 1, NONE) == SB_SAVE_IO_ERROR);
    CHECK(rmdir(path) == 0);
    CHECK(mkfifo(path, 0600) == 0);
    CHECK(save(root, uuid, bytes, 1, NONE) == SB_SAVE_IO_ERROR);
    CHECK(unlink(path) == 0);
    CHECK(symlink(otherpath, path) == 0);
    CHECK(save(root, uuid, bytes, 1, NONE) == SB_SAVE_IO_ERROR);
    CHECK(matches(root, other, old, sizeof(old)));
    CHECK(unlink(path) == 0);

    /* Preserve a colliding temporary entry; choose a fresh exclusive name. */
    char collision[256];
    snprintf(collision, sizeof(collision), "%s/.123e4567-e89b-42d3-a456-426614174000.sav.tmp.%ld.0",
             root, (long)getpid());
    int fd = open(collision, O_WRONLY | O_CREAT | O_EXCL, 0600);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(save(root, uuid, bytes, 513, NONE) == SB_SAVE_OK);
    struct stat st;
    CHECK(stat(collision, &st) == 0 && st.st_size == 0);
    CHECK(temporaries(root, true) == 1);
    CHECK(matches(root, uuid, bytes, 513));
    CHECK(chmod(root, 0500) == 0);
    if (geteuid() != 0) {
        CHECK(save(root, uuid, old, sizeof(old), NONE) == SB_SAVE_IO_ERROR);
        CHECK(matches(root, uuid, bytes, 513));
    } else {
        puts("Permission-denial check skipped for root.");
    }
    CHECK(chmod(root, 0700) == 0);
    CHECK(unlink(path) == 0 && unlink(otherpath) == 0);
    CHECK(rmdir(root) == 0);
    puts("Save writer and injected-failure checks passed");
    return 0;
}
