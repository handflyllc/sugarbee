#define _POSIX_C_SOURCE 200809L
#include "sb_save.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Faults are injected only in this executable, while prepare() is running. */
int __real_mkdirat(int fd, const char *path, mode_t mode);
int __real_openat(int fd, const char *path, int flags, ...);
int __real_fsync(int fd);
int __real_close(int fd);

typedef enum { NONE, MKDIR_FAIL, OPEN_FAIL, CHILD_SYNC_FAIL, PARENT_SYNC_FAIL,
               CHILD_CLOSE_FAIL, PARENT_CLOSE_FAIL, SYNC_INTERRUPTED } fault_t;
static fault_t fault;
static bool active, bad_order;
static struct stat parent_info;
static unsigned child_syncs, parent_syncs, child_closes, parent_closes, injected;
static bool child_synced;

static bool is_parent(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0 && st.st_dev == parent_info.st_dev &&
           st.st_ino == parent_info.st_ino;
}

int __wrap_mkdirat(int fd, const char *path, mode_t mode) {
    if (active && fault == MKDIR_FAIL) {
        ++injected; errno = EACCES; return -1;
    }
    return __real_mkdirat(fd, path, mode);
}

int __wrap_openat(int fd, const char *path, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    if (active && fault == OPEN_FAIL) {
        ++injected; errno = EACCES; return -1;
    }
    return __real_openat(fd, path, flags, mode);
}

int __wrap_fsync(int fd) {
    if (!active) return __real_fsync(fd);
    bool parent = is_parent(fd);
    if (parent) {
        ++parent_syncs;
        if (!child_synced) bad_order = true;
    } else {
        ++child_syncs;
    }
    if (fault == SYNC_INTERRUPTED && ((parent && parent_syncs == 1) ||
                                      (!parent && child_syncs == 1))) {
        ++injected; errno = EINTR; return -1;
    }
    if ((parent && fault == PARENT_SYNC_FAIL) || (!parent && fault == CHILD_SYNC_FAIL)) {
        ++injected; errno = EIO; return -1;
    }
    int result = __real_fsync(fd);
    if (!parent && result == 0) child_synced = true;
    return result;
}

int __wrap_close(int fd) {
    bool parent = active && is_parent(fd);
    int result = __real_close(fd);
    if (active) {
        if (parent) ++parent_closes;
        else ++child_closes;
        if ((parent && fault == PARENT_CLOSE_FAIL) || (!parent && fault == CHILD_CLOSE_FAIL)) {
            ++injected; errno = EIO; return -1;
        }
    }
    return result;
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Save directory check failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static sb_save_result_t prepare(const char *parent, const char *child, fault_t requested) {
    fault = requested;
    child_syncs = parent_syncs = child_closes = parent_closes = injected = 0;
    child_synced = bad_order = false;
    active = true;
    sb_save_result_t result = sb_save_prepare_directory(parent, child);
    active = false;
    return result;
}

static bool complete(void) {
    return child_syncs == 1 && parent_syncs == 1 && child_closes == 1 &&
           parent_closes == 1 && !bad_order;
}

int main(void) {
    char root[] = "/tmp/sugarbee-save-directory-XXXXXX";
    CHECK(mkdtemp(root));
    CHECK(stat(root, &parent_info) == 0);
    char path[256], data_path[256], missing[256], alias[256];
    snprintf(path, sizeof(path), "%s/saves", root);
    snprintf(data_path, sizeof(data_path), "%s/saves/existing.sav", root);
    snprintf(missing, sizeof(missing), "%s/absent", root);
    snprintf(alias, sizeof(alias), "%s/alias", root);

    const char *invalid[] = {NULL, "", ".", "..", "/absolute", "a/b", "saves/"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(prepare(root, invalid[i], NONE) == SB_SAVE_INVALID_ARGUMENT);
        CHECK(child_syncs == 0 && parent_syncs == 0);
    }
    CHECK(prepare(NULL, "saves", NONE) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(prepare("", "saves", NONE) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(prepare(missing, "saves", NONE) == SB_SAVE_IO_ERROR);
    CHECK(access(missing, F_OK) != 0 && access(path, F_OK) != 0);

    mode_t old_mask = umask(0077);
    CHECK(prepare(root, "saves", NONE) == SB_SAVE_OK && complete());
    umask(old_mask);
    struct stat st;
    CHECK(stat(path, &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & 0777) == 0700);
    CHECK(chmod(path, 0750) == 0);
    CHECK(prepare(root, "saves", NONE) == SB_SAVE_OK && complete());
    CHECK(stat(path, &st) == 0 && (st.st_mode & 0777) == 0750);

    const unsigned char bytes[] = {0, 1, 255, 2, 0};
    FILE *file = fopen(data_path, "wb");
    CHECK(file && fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    CHECK(fclose(file) == 0);
    struct stat before;
    CHECK(stat(data_path, &before) == 0);

    /* Existing data survives preparation failures, then a complete retry. */
    const fault_t failures[] = {MKDIR_FAIL, OPEN_FAIL, CHILD_SYNC_FAIL,
        PARENT_SYNC_FAIL, CHILD_CLOSE_FAIL, PARENT_CLOSE_FAIL};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        CHECK(prepare(root, "saves", failures[i]) == SB_SAVE_IO_ERROR);
        CHECK(injected == 1 && parent_closes == 1 && !bad_order);
        CHECK(child_closes == (failures[i] == MKDIR_FAIL || failures[i] == OPEN_FAIL ? 0u : 1u));
        CHECK(prepare(root, "saves", NONE) == SB_SAVE_OK && complete());
        CHECK(stat(data_path, &st) == 0 && st.st_ino == before.st_ino && st.st_size == before.st_size);
        unsigned char readback[sizeof(bytes)];
        file = fopen(data_path, "rb");
        CHECK(file && fread(readback, 1, sizeof(readback), file) == sizeof(readback));
        CHECK(fclose(file) == 0 && memcmp(readback, bytes, sizeof(bytes)) == 0);
    }
    CHECK(prepare(root, "saves", SYNC_INTERRUPTED) == SB_SAVE_OK);
    CHECK(injected == 2 && child_syncs == 2 && parent_syncs == 2 && !bad_order);
    CHECK(child_closes == 1 && parent_closes == 1);
    CHECK(unlink(data_path) == 0 && rmdir(path) == 0);

    /* Failure after creation leaves the directory for a fully synchronized retry. */
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        CHECK(prepare(root, "saves", failures[i]) == SB_SAVE_IO_ERROR && injected == 1);
        if (failures[i] == MKDIR_FAIL) CHECK(access(path, F_OK) != 0);
        else CHECK(stat(path, &st) == 0 && S_ISDIR(st.st_mode));
        CHECK(prepare(root, "saves", NONE) == SB_SAVE_OK && complete());
        CHECK(rmdir(path) == 0);
    }

    file = fopen(path, "wb");
    CHECK(file && fclose(file) == 0);
    CHECK(prepare(root, "saves", NONE) == SB_SAVE_IO_ERROR);
    CHECK(prepare(path, "child", NONE) == SB_SAVE_IO_ERROR);
    CHECK(stat(path, &st) == 0 && S_ISREG(st.st_mode));
    CHECK(unlink(path) == 0);
    CHECK(symlink(root, path) == 0);
    CHECK(prepare(root, "saves", NONE) == SB_SAVE_IO_ERROR);
    CHECK(lstat(path, &st) == 0 && S_ISLNK(st.st_mode));
    CHECK(unlink(path) == 0);
    CHECK(symlink("missing-target", path) == 0);
    CHECK(prepare(root, "saves", NONE) == SB_SAVE_IO_ERROR);
    CHECK(unlink(path) == 0);
    CHECK(mkfifo(path, 0600) == 0);
    CHECK(prepare(root, "saves", NONE) == SB_SAVE_IO_ERROR);
    CHECK(unlink(path) == 0);

    /* Host-selected parent aliases are supported; only the child is no-follow. */
    CHECK(symlink(root, alias) == 0);
    CHECK(prepare(alias, "saves", NONE) == SB_SAVE_OK && complete());
    CHECK(rmdir(path) == 0 && unlink(alias) == 0);
    CHECK(chmod(root, 0500) == 0);
    if (geteuid() != 0) {
        CHECK(prepare(root, "saves", NONE) == SB_SAVE_IO_ERROR);
        CHECK(access(path, F_OK) != 0);
    } else puts("Permission-denial check skipped for root.");
    CHECK(chmod(root, 0700) == 0 && rmdir(root) == 0);
    puts("One-level save directory and failure checks passed");
    return 0;
}
