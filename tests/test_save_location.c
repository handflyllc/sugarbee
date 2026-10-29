#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include "sb_save.h"

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int __real_fsync(int fd);
int __real_mkdirat(int fd, const char *name, mode_t mode);
static unsigned mkdir_calls, sync_calls, retry_target_syncs, injected;
static bool fail_parent_sync;
static struct stat parent_info;
static char pending[4096];

int __wrap_mkdirat(int fd, const char *name, mode_t mode) {
    ++mkdir_calls;
    return __real_mkdirat(fd, name, mode);
}
int __wrap_fsync(int fd) {
    ++sync_calls;
    struct stat st;
    if (fstat(fd, &st) == 0 && st.st_dev == parent_info.st_dev && st.st_ino == parent_info.st_ino) {
        ++retry_target_syncs;
        if (fail_parent_sync && access(pending, F_OK) == 0) {
            ++injected; errno = EIO; return -1;
        }
    }
    return __real_fsync(fd);
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Save location check failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static bool resolve(const char *selected, const char *absolute) {
    sb_save_location_t location;
    unsigned before_mkdir = mkdir_calls, before_sync = sync_calls;
    sb_save_result_t result = sb_save_resolve_location(selected, &location);
    if (result != SB_SAVE_OK || strcmp(location.relative, absolute + 1) != 0 ||
        before_mkdir != mkdir_calls || before_sync != sync_calls) {
        fprintf(stderr, "Unexpected resolution of %s (result %d)\n", selected, result);
        return false;
    }
    return true;
}
static bool reject(const char *selected, sb_save_result_t expected) {
    sb_save_location_t location;
    strcpy(location.relative, "stale destination");
    unsigned before_mkdir = mkdir_calls, before_sync = sync_calls;
    sb_save_result_t result = sb_save_resolve_location(selected, &location);
    return result == expected && !location.relative[0] && before_mkdir == mkdir_calls &&
           before_sync == sync_calls;
}
static int entries(const char *path) {
    DIR *dir = opendir(path); if (!dir) return -1;
    int count = 0; struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    }
    if (closedir(dir) != 0) return -1;
    return count;
}

int main(void) {
    char root[] = "/tmp/sugarbee-save-location-XXXXXX";
    CHECK(mkdtemp(root));
    char physical[4096], deep[4096], link[4096], candidate[4096], expected[4096];
    char dangling[4096], loop[4096], plain[4096], blocked[4096];
    snprintf(physical, sizeof(physical), "%s/physical", root);
    snprintf(deep, sizeof(deep), "%s/physical/deep", root);
    snprintf(link, sizeof(link), "%s/link", root);
    snprintf(dangling, sizeof(dangling), "%s/dangling", root);
    snprintf(loop, sizeof(loop), "%s/loop", root);
    snprintf(plain, sizeof(plain), "%s/plain", root);
    snprintf(blocked, sizeof(blocked), "%s/blocked", root);
    CHECK(mkdir(physical, 0700) == 0 && mkdir(deep, 0700) == 0);
    /* Relative symlink target simulates a relocated HOME or data directory. */
    CHECK(symlink("physical/deep", link) == 0);
    CHECK(resolve(link, deep));
    snprintf(candidate, sizeof(candidate), "%s/link/../new saves", root);
    snprintf(expected, sizeof(expected), "%s/physical/new saves", root);
    CHECK(resolve(candidate, expected));
    CHECK(access(expected, F_OK) != 0);
    snprintf(candidate, sizeof(candidate), "%s//link/./", root);
    CHECK(resolve(candidate, deep));
    snprintf(candidate, sizeof(candidate), "%s/link/new//saves/", root);
    snprintf(expected, sizeof(expected), "%s/physical/deep/new/saves", root);
    CHECK(resolve(candidate, expected));
    CHECK(entries(deep) == 0);
    snprintf(candidate, sizeof(candidate), "%s/missing/../saves", root);
    CHECK(reject(candidate, SB_SAVE_INVALID_ARGUMENT));
    snprintf(candidate, sizeof(candidate), "%s/missing/./saves", root);
    CHECK(reject(candidate, SB_SAVE_INVALID_ARGUMENT));
    snprintf(candidate, sizeof(candidate), "%s/link/missing/../saves", root);
    CHECK(reject(candidate, SB_SAVE_INVALID_ARGUMENT));
    CHECK(reject(NULL, SB_SAVE_INVALID_ARGUMENT));
    CHECK(reject("", SB_SAVE_INVALID_ARGUMENT));
    CHECK(reject("relative", SB_SAVE_INVALID_ARGUMENT));
    CHECK(reject("/", SB_SAVE_INVALID_ARGUMENT));
    CHECK(sb_save_resolve_location(root, NULL) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(symlink("absent", dangling) == 0);
    CHECK(reject(dangling, SB_SAVE_IO_ERROR));
    snprintf(candidate, sizeof(candidate), "%s/dangling/saves", root);
    CHECK(reject(candidate, SB_SAVE_IO_ERROR));
    CHECK(symlink("loop", loop) == 0 && reject(loop, SB_SAVE_IO_ERROR));
    FILE *file = fopen(plain, "wb"); CHECK(file && fclose(file) == 0);
    CHECK(reject(plain, SB_SAVE_IO_ERROR));
    snprintf(candidate, sizeof(candidate), "%s/plain/saves", root);
    CHECK(reject(candidate, SB_SAVE_IO_ERROR));
    CHECK(mkdir(blocked, 0000) == 0);
    if (geteuid() != 0) {
        snprintf(candidate, sizeof(candidate), "%s/blocked/saves", root);
        CHECK(reject(candidate, SB_SAVE_IO_ERROR));
    } else puts("Permission-denial check skipped for root.");
    CHECK(chmod(blocked, 0700) == 0 && rmdir(blocked) == 0);

    /* Bounds apply to the resolved relative path, including existing ancestors. */
    char over[SB_SAVE_MAX_SELECTED_PATH_BYTES + 2u];
    memset(over, 'a', sizeof(over)-1); over[0] = '/'; over[sizeof(over)-1] = 0;
    CHECK(reject(over, SB_SAVE_INVALID_ARGUMENT));
    snprintf(candidate, sizeof(candidate), "%s/", root);
    size_t used = strlen(candidate);
    memset(candidate + used, 'x', 256); candidate[used + 256] = 0;
    CHECK(reject(candidate, SB_SAVE_INVALID_ARGUMENT));
    snprintf(candidate, sizeof(candidate), "%s", root);
    used = strlen(candidate);
    /* root is /tmp/<one name>: add thirty components to reach exactly 32. */
    for (int i = 0; i < 30; ++i) { candidate[used++] = '/'; candidate[used++] = 'x'; }
    candidate[used] = 0;
    CHECK(resolve(candidate, candidate));
    candidate[used++] = '/'; candidate[used++] = 'x'; candidate[used] = 0;
    CHECK(reject(candidate, SB_SAVE_INVALID_ARGUMENT));
    snprintf(candidate, sizeof(candidate), "%s", root);
    used = strlen(candidate);
    while (used < SB_SAVE_MAX_PATH_BYTES + 1u) {
        size_t remaining = SB_SAVE_MAX_PATH_BYTES + 1u - used;
        size_t part = remaining > 256 ? 255 : remaining - 1;
        if (remaining == 257) part = 254;
        candidate[used++] = '/'; memset(candidate + used, 'y', part); used += part;
    }
    candidate[used] = 0;
    CHECK(resolve(candidate, candidate)); /* Exactly 1024 relative bytes. */
    candidate[used++] = 'z'; candidate[used] = 0;
    CHECK(reject(candidate, SB_SAVE_INVALID_ARGUMENT));
    CHECK(entries(deep) == 0 && entries(physical) == 1);

    /* A visible directory left by failed preparation is not treated as a new
       durable anchor on retry. The same full root-relative path is retained. */
    snprintf(candidate, sizeof(candidate), "%s/link/pending", root);
    snprintf(pending, sizeof(pending), "%s/physical/deep/pending", root);
    sb_save_location_t before, after;
    CHECK(sb_save_resolve_location(candidate, &before) == SB_SAVE_OK);
    CHECK(stat(deep, &parent_info) == 0);
    fail_parent_sync = true;
    CHECK(sb_save_prepare_path("/", before.relative) == SB_SAVE_IO_ERROR && injected == 1);
    CHECK(access(pending, F_OK) == 0);
    CHECK(sb_save_resolve_location(candidate, &after) == SB_SAVE_OK);
    CHECK(strcmp(before.relative, after.relative) == 0);
    fail_parent_sync = false; retry_target_syncs = 0;
    CHECK(sb_save_prepare_path("/", after.relative) == SB_SAVE_OK);
    CHECK(retry_target_syncs == 2); /* deep as child, then as pending's parent. */
    CHECK(rmdir(pending) == 0);
    CHECK(unlink(link) == 0 && unlink(dangling) == 0 && unlink(loop) == 0 && unlink(plain) == 0);
    CHECK(rmdir(deep) == 0 && rmdir(physical) == 0 && rmdir(root) == 0);
    puts("Read-only save location and preparation-retry checks passed");
    return 0;
}
