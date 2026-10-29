#define _POSIX_C_SOURCE 200809L
#include "sb_save.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int __real_mkdirat(int fd, const char *path, mode_t mode);
int __real_openat(int fd, const char *path, int flags, ...);
int __real_fsync(int fd);
int __real_close(int fd);
ssize_t __real_write(int fd, const void *data, size_t size);
int __real_renameat(int oldfd, const char *oldname, int newfd, const char *newname);

typedef enum { NONE, MKDIR_SECOND, OPEN_SECOND, SYNC_CHILD_SECOND,
               SYNC_PARENT_SECOND, CLOSE_SECOND, WRITE_FAIL, FILE_SYNC_FAIL,
               RENAME_FAIL, FINAL_SYNC_FAIL } fault_t;
static bool active, renamed, order_error;
static fault_t fault;
static unsigned mkdirs, directory_opens, directory_syncs, closes;
static unsigned temp_opens, writes, renames, injected;
static int expected_levels;

int __wrap_mkdirat(int fd, const char *path, mode_t mode) {
    if (active && ++mkdirs == 2 && fault == MKDIR_SECOND) {
        ++injected; errno = EACCES; return -1;
    }
    return __real_mkdirat(fd, path, mode);
}
int __wrap_openat(int fd, const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap; va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap);
    }
    if (active) {
        if (flags & O_DIRECTORY) {
            if (++directory_opens == 2 && fault == OPEN_SECOND) {
                ++injected; errno = EACCES; return -1;
            }
        } else if (flags & O_CREAT) {
            ++temp_opens;
            if (directory_syncs != (unsigned)(2 * expected_levels)) order_error = true;
        }
    }
    return __real_openat(fd, path, flags, mode);
}
int __wrap_fsync(int fd) {
    if (active) {
        struct stat st;
        if (fstat(fd, &st) != 0) return -1;
        if (S_ISDIR(st.st_mode)) {
            ++directory_syncs;
            if ((fault == SYNC_CHILD_SECOND && directory_syncs == 3) ||
                (fault == SYNC_PARENT_SECOND && directory_syncs == 4) ||
                (fault == FINAL_SYNC_FAIL && renamed)) {
                ++injected; errno = EIO; return -1;
            }
        } else if (fault == FILE_SYNC_FAIL) {
            ++injected; errno = EIO; return -1;
        }
    }
    return __real_fsync(fd);
}
int __wrap_close(int fd) {
    int result = __real_close(fd);
    if (active && ++closes == 2 && fault == CLOSE_SECOND) {
        ++injected; errno = EIO; return -1;
    }
    return result;
}
ssize_t __wrap_write(int fd, const void *data, size_t size) {
    if (active) {
        ++writes;
        if (fault == WRITE_FAIL) { ++injected; errno = ENOSPC; return -1; }
    }
    return __real_write(fd, data, size);
}
int __wrap_renameat(int oldfd, const char *oldname, int newfd, const char *newname) {
    if (active) {
        ++renames;
        if (fault == RENAME_FAIL) { ++injected; errno = EIO; return -1; }
    }
    int result = __real_renameat(oldfd, oldname, newfd, newname);
    if (active && result == 0) renamed = true;
    return result;
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Nested save check failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)
static const uint8_t uuid[16] = {0};
static const char *save_name = "00000000-0000-0000-0000-000000000000.sav";

static void begin(fault_t requested, int levels) {
    fault = requested; expected_levels = levels;
    mkdirs = directory_opens = directory_syncs = closes = 0;
    temp_opens = writes = renames = injected = 0;
    renamed = order_error = false;
    active = true;
}
static sb_save_result_t prepare(const char *base, const char *path, int levels) {
    begin(NONE, levels);
    sb_save_result_t result = sb_save_prepare_path(base, path);
    active = false;
    return result;
}
static sb_save_result_t save(const char *base, const char *path, const uint8_t *id,
                             const uint8_t *data, size_t size, fault_t requested) {
    begin(requested, 2);
    sb_save_result_t result = sb_save_write_prepared(base, path, id, data, size);
    active = false;
    return result;
}
static bool matches(const char *directory, const uint8_t *expected, size_t size) {
    uint8_t *data = NULL; size_t length = 0;
    sb_save_result_t result = sb_save_read(directory, uuid, &data, &length);
    bool ok = result == SB_SAVE_OK && length == size &&
              (size == 0 || memcmp(data, expected, size) == 0);
    free(data); return ok;
}

/* Remove only entries under this test's mkdtemp root, never follow symlinks. */
static bool clear_directory(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return false;
    bool ok = true;
    struct dirent *entry;
    while (ok && (entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[2048];
        int n = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        struct stat st;
        if (n < 0 || (size_t)n >= sizeof(child) || lstat(child, &st) != 0) { ok = false; break; }
        if (S_ISDIR(st.st_mode)) ok = clear_directory(child) && rmdir(child) == 0;
        else ok = unlink(child) == 0;
    }
    return closedir(dir) == 0 && ok;
}
static int entry_count(const char *path) {
    DIR *dir = opendir(path); if (!dir) return -1;
    int count = 0; struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    }
    if (closedir(dir) != 0) return -1;
    return count;
}

int main(void) {
    char root[] = "/tmp/sugarbee-save-path-XXXXXX";
    CHECK(mkdtemp(root));
    char first[256], leaf[256], destination[320], missing[256];
    snprintf(first, sizeof(first), "%s/game", root);
    snprintf(leaf, sizeof(leaf), "%s/game/saves", root);
    snprintf(destination, sizeof(destination), "%s/%s", leaf, save_name);
    snprintf(missing, sizeof(missing), "%s/missing", root);
    uint8_t bytes[SB_SAVE_MAX_BYTES + 1u];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)i;
    const uint8_t old[] = "old save";

    /* Every invalid path is rejected before even the first mkdir. */
    const char *invalid[] = {NULL, "", "/absolute", ".", "..", "game/..", "game/.",
                             "game//saves", "game/saves/", "game/saves/../bad"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(prepare(root, invalid[i], 0) == SB_SAVE_INVALID_ARGUMENT && mkdirs == 0);
        CHECK(save(root, invalid[i], uuid, bytes, 5, NONE) == SB_SAVE_INVALID_ARGUMENT);
        CHECK(mkdirs == 0 && temp_opens == 0 && entry_count(root) == 0);
    }
    CHECK(prepare(NULL, "game/saves", 2) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(prepare("", "game/saves", 2) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(save(root, "game/saves", uuid, bytes, sizeof(bytes), NONE) == SB_SAVE_TOO_LARGE);
    CHECK(mkdirs == 0 && entry_count(root) == 0);
    CHECK(save(root, "game/saves", NULL, bytes, 1, NONE) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(save(root, "game/saves", uuid, NULL, 1, NONE) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(entry_count(root) == 0);
    CHECK(prepare(missing, "game/saves", 2) == SB_SAVE_IO_ERROR);
    CHECK(access(missing, F_OK) != 0);

    char longpath[SB_SAVE_MAX_PATH_BYTES + 2u];
    memset(longpath, 'a', sizeof(longpath)-1); longpath[sizeof(longpath)-1] = 0;
    CHECK(prepare(root, longpath, 0) == SB_SAVE_INVALID_ARGUMENT && mkdirs == 0);
    memset(longpath, 'a', SB_SAVE_MAX_COMPONENT_BYTES + 1u);
    longpath[SB_SAVE_MAX_COMPONENT_BYTES + 1u] = 0;
    CHECK(prepare(root, longpath, 0) == SB_SAVE_INVALID_ARGUMENT && mkdirs == 0);
    size_t offset = 0;
    for (unsigned i = 0; i < SB_SAVE_MAX_COMPONENTS + 1u; ++i) {
        if (i) longpath[offset++] = '/';
        longpath[offset++] = 'a';
    }
    longpath[offset] = 0;
    CHECK(prepare(root, longpath, 0) == SB_SAVE_INVALID_ARGUMENT && mkdirs == 0);
    longpath[offset-2] = 0; /* Exactly 32 components. */
    CHECK(prepare(root, longpath, 32) == SB_SAVE_OK && directory_syncs == 64);
    CHECK(clear_directory(root));
    /* Exactly 1024 bytes, with each component within 255 bytes. */
    const size_t lengths[] = {255,255,255,254,1};
    offset = 0;
    for (size_t i = 0; i < 5; ++i) {
        if (i) longpath[offset++] = '/';
        memset(longpath + offset, 'b', lengths[i]); offset += lengths[i];
    }
    longpath[offset] = 0;
    CHECK(offset == SB_SAVE_MAX_PATH_BYTES);
    CHECK(prepare(root, longpath, 5) == SB_SAVE_OK && directory_syncs == 10);
    CHECK(clear_directory(root));

    CHECK(prepare(root, "game/saves", 2) == SB_SAVE_OK && directory_syncs == 4);
    CHECK(temp_opens == 0 && writes == 0);
    CHECK(chmod(first, 0750) == 0 && chmod(leaf, 0750) == 0);
    CHECK(prepare(root, "game/saves", 2) == SB_SAVE_OK && directory_syncs == 4);
    struct stat st;
    CHECK(stat(first, &st) == 0 && (st.st_mode & 0777) == 0750);
    CHECK(stat(leaf, &st) == 0 && (st.st_mode & 0777) == 0750);
    CHECK(rmdir(leaf) == 0); /* Partially existing path. */
    CHECK(prepare(root, "game/saves", 2) == SB_SAVE_OK && directory_syncs == 4);
    CHECK(clear_directory(root));

    /* Completely missing nested path through the coordinator, and replacement. */
    const size_t sizes[] = {0, 513, SB_SAVE_MAX_BYTES};
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
        CHECK(save(root, "game/saves", uuid, sizes[i] ? bytes : NULL, sizes[i], NONE) == SB_SAVE_OK);
        CHECK(!order_error && directory_syncs == 5 && temp_opens == 1 && renames == 1);
        CHECK(matches(leaf, bytes, sizes[i]) && entry_count(leaf) == 1);
    }
    CHECK(clear_directory(root));

    const fault_t prep_failures[] = {MKDIR_SECOND, OPEN_SECOND, SYNC_CHILD_SECOND,
                                     SYNC_PARENT_SECOND, CLOSE_SECOND};
    for (size_t i = 0; i < sizeof(prep_failures)/sizeof(prep_failures[0]); ++i) {
        /* New partial path survives failure; writer must never be entered. */
        CHECK(save(root, "game/saves", uuid, bytes, 513, prep_failures[i]) == SB_SAVE_IO_ERROR);
        CHECK(injected == 1 && temp_opens == 0 && writes == 0 && renames == 0);
        CHECK(access(first, F_OK) == 0 && access(destination, F_OK) != 0);
        CHECK(save(root, "game/saves", uuid, old, sizeof(old), NONE) == SB_SAVE_OK);
        CHECK(directory_syncs == 5 && !order_error && matches(leaf, old, sizeof(old)));
        /* Same failure with existing save: preserve contents and inode. */
        struct stat before; CHECK(stat(destination, &before) == 0);
        CHECK(save(root, "game/saves", uuid, bytes, 513, prep_failures[i]) == SB_SAVE_IO_ERROR);
        CHECK(injected == 1 && temp_opens == 0 && writes == 0 && renames == 0);
        CHECK(stat(destination, &st) == 0 && st.st_ino == before.st_ino);
        CHECK(matches(leaf, old, sizeof(old)));
        CHECK(save(root, "game/saves", uuid, bytes, 513, NONE) == SB_SAVE_OK);
        CHECK(directory_syncs == 5 && !order_error && matches(leaf, bytes, 513));
        CHECK(clear_directory(root));
    }
    const fault_t write_failures[] = {WRITE_FAIL, FILE_SYNC_FAIL, RENAME_FAIL, FINAL_SYNC_FAIL};
    for (size_t i = 0; i < sizeof(write_failures)/sizeof(write_failures[0]); ++i) {
        CHECK(save(root, "game/saves", uuid, old, sizeof(old), NONE) == SB_SAVE_OK);
        bool uncertain = write_failures[i] == FINAL_SYNC_FAIL;
        CHECK(save(root, "game/saves", uuid, bytes, 513, write_failures[i]) ==
              (uncertain ? SB_SAVE_DURABILITY_UNCERTAIN : SB_SAVE_IO_ERROR));
        CHECK(injected == 1 && !order_error && renamed == uncertain);
        CHECK(matches(leaf, uncertain ? bytes : old, uncertain ? 513 : sizeof(old)));
        CHECK(entry_count(leaf) == 1);
    }
    CHECK(clear_directory(root));
    /* Reject symlinks at either level, without following or replacing them. */
    CHECK(symlink(root, first) == 0);
    CHECK(save(root, "game/saves", uuid, bytes, 1, NONE) == SB_SAVE_IO_ERROR && temp_opens == 0);
    CHECK(lstat(first, &st) == 0 && S_ISLNK(st.st_mode));
    CHECK(unlink(first) == 0 && mkdir(first, 0700) == 0);
    CHECK(symlink(root, leaf) == 0);
    CHECK(prepare(root, "game/saves", 2) == SB_SAVE_IO_ERROR);
    CHECK(lstat(leaf, &st) == 0 && S_ISLNK(st.st_mode));
    CHECK(unlink(leaf) == 0);
    int fd = open(leaf, O_WRONLY | O_CREAT | O_EXCL, 0600);
    CHECK(fd >= 0 && close(fd) == 0);
    CHECK(save(root, "game/saves", uuid, bytes, 1, NONE) == SB_SAVE_IO_ERROR && temp_opens == 0);
    CHECK(stat(leaf, &st) == 0 && S_ISREG(st.st_mode));
    CHECK(clear_directory(root) && rmdir(root) == 0);
    puts("Nested preparation, coordinator, and failure/retry checks passed");
    return 0;
}
