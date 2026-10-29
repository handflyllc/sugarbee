#define _POSIX_C_SOURCE 200809L
#include "sb_save.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Link-time wrappers exist only in this test executable. SIGSTOP rendezvous
   tells the parent the real filesystem operation reached the requested point.
   The parent then uses SIGKILL, so no writer cleanup can run. */
ssize_t __real_write(int fd, const void *data, size_t size);
int __real_fsync(int fd);
int __real_renameat(int oldfd, const char *oldname, int newfd, const char *newname);
static enum { NONE, PARTIAL_WRITE, FILE_SYNCED, REPLACED } interruption;

static void stop_writer(void) {
    if (raise(SIGSTOP) != 0) _exit(90);
    _exit(91); /* The parent must kill us, never resume normal execution. */
}
ssize_t __wrap_write(int fd, const void *data, size_t size) {
    if (interruption == PARTIAL_WRITE) {
        if (size <= 7 || __real_write(fd, data, 7) != 7) _exit(92);
        stop_writer();
    }
    return __real_write(fd, data, size);
}
int __wrap_fsync(int fd) {
    int result = __real_fsync(fd);
    struct stat st;
    if (interruption == FILE_SYNCED && result == 0 &&
        fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) stop_writer();
    return result;
}
int __wrap_renameat(int oldfd, const char *oldname, int newfd, const char *newname) {
    int result = __real_renameat(oldfd, oldname, newfd, newname);
    if (interruption == REPLACED && result == 0) stop_writer();
    return result;
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Interruption check failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static const uint8_t uuid[16] = {0x42, 0x17};
static uint8_t old_data[41], new_data[SB_SAVE_MAX_BYTES], recovered_data[83];

static pid_t wait_child(pid_t child, int *status, int options) {
    pid_t result;
    do { result = waitpid(child, status, options); } while (result < 0 && errno == EINTR);
    return result;
}
static bool matches(const char *directory, const uint8_t *expected, size_t size) {
    uint8_t *data = NULL;
    size_t length = 0;
    sb_save_result_t result = sb_save_read(directory, uuid, &data, &length);
    bool ok = result == SB_SAVE_OK && length == size && memcmp(data, expected, size) == 0;
    free(data);
    return ok;
}
/* Each invocation forks a fresh reader; recovery writes only after checking
   the interrupted result. A separate fresh reader verifies that recovery. */
static bool fresh_process(const char *directory, const uint8_t *expected,
                          size_t size, bool recover) {
    pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        interruption = NONE;
        if (!matches(directory, expected, size)) _exit(2);
        if (recover && sb_save_write(directory, uuid, recovered_data,
                                    sizeof(recovered_data)) != SB_SAVE_OK) _exit(3);
        _exit(0);
    }
    int status;
    return wait_child(child, &status, 0) == child && WIFEXITED(status) &&
           WEXITSTATUS(status) == 0;
}
/* Check the abandoned temp's exact contents, not just its presence. The final
   save is always loaded via sb_save_read, which must ignore this other file. */
static bool check_leftovers(const char *directory, size_t expected_size) {
    DIR *dir = opendir(directory);
    if (!dir) return false;
    unsigned count = 0;
    bool ok = true;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (!strstr(entry->d_name, ".tmp.")) continue;
        ++count;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        FILE *file = fopen(path, "rb");
        if (!file) { ok = false; continue; }
        size_t i = 0;
        int byte;
        while ((byte = fgetc(file)) != EOF) {
            if (i >= expected_size || byte != new_data[i]) ok = false;
            ++i;
        }
        if (ferror(file) || i != expected_size) ok = false;
        if (fclose(file) != 0) ok = false;
    }
    if (closedir(dir) != 0) ok = false;
    return ok && count == (expected_size ? 1u : 0u);
}
static bool remove_fixture(const char *directory) {
    DIR *dir = opendir(directory);
    if (!dir) return false;
    bool ok = true;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (unlink(path) != 0) ok = false;
    }
    if (closedir(dir) != 0) ok = false;
    return ok && rmdir(directory) == 0;
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    int point;
    if (!strcmp(argv[1], "partial_write")) point = PARTIAL_WRITE;
    else if (!strcmp(argv[1], "file_synced")) point = FILE_SYNCED;
    else if (!strcmp(argv[1], "replaced")) point = REPLACED;
    else return 1;
    /* Distinct binary payloads of different lengths expose partial/mixed data. */
    memset(old_data, 0x11, sizeof(old_data));
    for (size_t i = 0; i < sizeof(new_data); ++i) new_data[i] = (uint8_t)i;
    memset(recovered_data, 0x73, sizeof(recovered_data));
    char directory[] = "/tmp/sugarbee-save-interruption-XXXXXX";
    CHECK(mkdtemp(directory));
    CHECK(sb_save_write(directory, uuid, old_data, sizeof(old_data)) == SB_SAVE_OK);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        interruption = point;
        (void)sb_save_write(directory, uuid, new_data, sizeof(new_data));
        _exit(4); /* Fails if the interception point was never reached. */
    }
    int status;
    CHECK(wait_child(child, &status, WUNTRACED) == child);
    bool reached = WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP;
    /* Reap even an unexpected stop; never leave a stopped writer behind. */
    if (WIFSTOPPED(status)) {
        CHECK(kill(child, SIGKILL) == 0);
        CHECK(wait_child(child, &status, 0) == child);
        CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL);
    }
    CHECK(reached);
    size_t leftover_size = point == PARTIAL_WRITE ? 7 :
                           point == FILE_SYNCED ? sizeof(new_data) : 0;
    CHECK(check_leftovers(directory, leftover_size));
    CHECK(fresh_process(directory, point == REPLACED ? new_data : old_data,
                        point == REPLACED ? sizeof(new_data) : sizeof(old_data), true));
    CHECK(fresh_process(directory, recovered_data, sizeof(recovered_data), false));
    CHECK(check_leftovers(directory, leftover_size));
    CHECK(remove_fixture(directory));
    printf("Interrupted save recovery passed: %s (process termination, not power loss)\n", argv[1]);
    return 0;
}
