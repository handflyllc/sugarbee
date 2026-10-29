#define _POSIX_C_SOURCE 200809L
#include "sb_save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Save reader check failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static const uint8_t uuid[16] = {
    0x12,0x3e,0x45,0x67,0xe8,0x9b,0x42,0xd3,
    0xa4,0x56,0x42,0x66,0x14,0x17,0x40,0x00
};

static int write_fixture(const char *path, const uint8_t *bytes, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(bytes, 1, size, f) == size;
    return fclose(f) == 0 && ok;
}

static int expect_failure(const char *root, const uint8_t *id,
                          sb_save_result_t expected) {
    uint8_t sentinel;
    uint8_t *data = &sentinel;
    size_t length = 99;
    sb_save_result_t result = sb_save_read(root, id, &data, &length);
    if (result != expected || data != NULL || length != 0) {
        fprintf(stderr, "Expected result %d and empty outputs; got %d, length %zu\n",
                expected, result, length);
        return 0;
    }
    return 1;
}

int main(void) {
    char root[] = "/tmp/sugarbee-save-reader-XXXXXX";
    CHECK(mkdtemp(root));
    char path[256], missing[256];
    /* Independent literal verifies canonical UUID-to-filename selection. */
    snprintf(path, sizeof(path), "%s/123e4567-e89b-42d3-a456-426614174000.sav", root);
    snprintf(missing, sizeof(missing), "%s/absent", root);
    CHECK(expect_failure(root, uuid, SB_SAVE_NOT_FOUND));
    CHECK(expect_failure(missing, uuid, SB_SAVE_NOT_FOUND));
    CHECK(access(missing, F_OK) != 0); /* Reader did not create a directory. */
    CHECK(expect_failure(NULL, uuid, SB_SAVE_INVALID_ARGUMENT));
    CHECK(expect_failure("", uuid, SB_SAVE_INVALID_ARGUMENT));
    CHECK(expect_failure(root, NULL, SB_SAVE_INVALID_ARGUMENT));
    size_t size = 99;
    CHECK(sb_save_read(root, uuid, NULL, &size) == SB_SAVE_INVALID_ARGUMENT && size == 0);
    uint8_t *data = NULL;
    CHECK(sb_save_read(root, uuid, &data, NULL) == SB_SAVE_INVALID_ARGUMENT && !data);

    uint8_t bytes[SB_SAVE_MAX_BYTES + 1u];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)(i % 256);
    const size_t sizes[] = {0, 1, 513, SB_SAVE_MAX_BYTES};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        CHECK(write_fixture(path, bytes, sizes[i]));
        CHECK(chmod(path, 0400) == 0); /* Read-only saves must remain readable. */
        for (int repeat = 0; repeat < 2; ++repeat) {
            CHECK(sb_save_read(root, uuid, &data, &size) == SB_SAVE_OK);
            CHECK(data && size == sizes[i] && memcmp(data, bytes, size) == 0);
            free(data);
            data = NULL;
        }
        struct stat st;
        CHECK(stat(path, &st) == 0 && st.st_size == (off_t)sizes[i]);
        CHECK(chmod(path, 0600) == 0);
    }
    uint8_t other[16];
    memcpy(other, uuid, sizeof(other)); other[15]++;
    CHECK(expect_failure(root, other, SB_SAVE_NOT_FOUND));
    CHECK(write_fixture(path, bytes, sizeof(bytes)));
    CHECK(expect_failure(root, uuid, SB_SAVE_TOO_LARGE));
    struct stat st;
    CHECK(stat(path, &st) == 0 && st.st_size == (off_t)sizeof(bytes));
    CHECK(write_fixture(path, bytes, 513));
    CHECK(chmod(path, 0000) == 0);
    if (geteuid() != 0) {
        CHECK(expect_failure(root, uuid, SB_SAVE_IO_ERROR));
    } else {
        puts("Permission-denial check skipped for root; special-file errors still checked.");
    }
    CHECK(chmod(path, 0600) == 0);
    CHECK(expect_failure(path, uuid, SB_SAVE_IO_ERROR)); /* Root is a file. */
    CHECK(unlink(path) == 0);
    CHECK(mkdir(path, 0700) == 0);
    CHECK(expect_failure(root, uuid, SB_SAVE_IO_ERROR));
    CHECK(rmdir(path) == 0);
    CHECK(mkfifo(path, 0600) == 0);
    CHECK(expect_failure(root, uuid, SB_SAVE_IO_ERROR)); /* Must not block. */
    CHECK(unlink(path) == 0);
    CHECK(symlink("absent-target", path) == 0);
    CHECK(expect_failure(root, uuid, SB_SAVE_IO_ERROR));
    CHECK(unlink(path) == 0);
    CHECK(rmdir(root) == 0);
    puts("Save reader checks passed");
    return 0;
}
