/*
 * Project SugarBee - Linux .vcart packager.
 * Creates a ZIP payload in a private temporary directory and prefixes it
 * with a 64-byte, little-endian SBEE header. No shell command construction.
 */
#define _POSIX_C_SOURCE 200809L

#include "vcart.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define TOOL_VERSION "0.1.0"

static void usage(const char* prog) {
    fprintf(stderr,
        "SugarBee Cartridge Packager v%s\n"
        "Usage: %s -i <game-dir> -o <game.vcart> -t <title> [options]\n"
        "  --os M.m.p      Minimum runtime version (default %d.%d.%d)\n"
        "  --init-identity Create game-dir/sugarbee.uuid once; requires only -i\n"
        "                  Optional --uuid adopts an existing game identity\n"
        "  --uuid UUID     Reuse a game's existing UUIDv4 across releases\n"
        "  -v              Verbose output\n"
        "  -h, --help      Show help\n",
        TOOL_VERSION, prog, VCART_OS_MAJOR, VCART_OS_MINOR, VCART_OS_PATCH);
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool parse_uuid(const char* str, uint8_t uuid[16]) {
    static const int hyphens[] = {8, 13, 18, 23};
    if (!str || strlen(str) != 36) return false;
    for (int i = 0, j = 0; i < 36; ) {
        bool is_hyphen = false;
        for (unsigned n = 0; n < sizeof(hyphens) / sizeof(hyphens[0]); n++) {
            if (i == hyphens[n]) is_hyphen = true;
        }
        if (is_hyphen) {
            if (str[i++] != '-') return false;
            continue;
        }
        int hi = hex_digit(str[i++]);
        int lo = hex_digit(str[i++]);
        if (hi < 0 || lo < 0) return false;
        uuid[j++] = (uint8_t)((hi << 4) | lo);
    }
    return (uuid[6] & 0xF0u) == 0x40u && (uuid[8] & 0xC0u) == 0x80u;
}

static bool generate_uuid(uint8_t uuid[16]) {
    FILE* random_file = fopen("/dev/urandom", "rb");
    if (!random_file) return false;
    bool ok = fread(uuid, 1, 16, random_file) == 16;
    fclose(random_file);
    if (!ok) return false;
    uuid[6] = (uint8_t)((uuid[6] & 0x0Fu) | 0x40u);
    uuid[8] = (uint8_t)((uuid[8] & 0x3Fu) | 0x80u);
    return true;
}

/* Source metadata only; the cartridge header remains the runtime authority. */
#define IDENTITY_FILE "sugarbee.uuid"

static bool resolve_identity(const char* input, const char* supplied, bool init,
                             uint8_t uuid[16], bool* persistent) {
    uint8_t explicit_uuid[16];
    if (supplied && !parse_uuid(supplied, explicit_uuid)) {
        fprintf(stderr, "Invalid UUIDv4 (expected canonical 8-4-4-4-12 form).\n");
        return false;
    }
    int dir = open(input, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir < 0) { perror("open game directory"); return false; }
    int fd = openat(dir, IDENTITY_FILE, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
        struct stat st;
        char text[38] = {0};
        bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
                  (st.st_size == 36 || st.st_size == 37);
        if (ok) {
            ssize_t n;
            do { n = read(fd, text, sizeof(text)); } while (n < 0 && errno == EINTR);
            ok = n == st.st_size && (n == 36 || text[36] == '\n');
            text[36] = '\0';
            ok = ok && parse_uuid(text, uuid);
        }
        if (close(fd) != 0) ok = false;
        close(dir);
        if (!ok) fprintf(stderr, "Invalid or unreadable %s: expected UUIDv4 with optional newline.\n", IDENTITY_FILE);
        else if (init) {
            fprintf(stderr, "%s already exists; initialization never overwrites identity.\n", IDENTITY_FILE);
            ok = false;
        } else if (supplied && memcmp(uuid, explicit_uuid, 16) != 0) {
            fprintf(stderr, "--uuid conflicts with %s; refusing to change save identity.\n", IDENTITY_FILE);
            ok = false;
        }
        *persistent = ok;
        return ok;
    }
    if (errno != ENOENT) {
        perror("read " IDENTITY_FILE);
        close(dir);
        return false;
    }
    if (supplied) memcpy(uuid, explicit_uuid, 16);
    else if (!generate_uuid(uuid)) {
        fprintf(stderr, "Unable to generate a random UUID.\n");
        close(dir);
        return false;
    }
    if (!init) { close(dir); return true; }
    fd = openat(dir, IDENTITY_FILE, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (fd < 0) { perror("create " IDENTITY_FILE); close(dir); return false; }
    char text[38];
    vcart_uuid_to_string(uuid, text);
    text[36] = '\n';
    size_t written = 0;
    while (written < 37) {
        ssize_t n = write(fd, text + written, 37 - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        written += (size_t)n;
    }
    bool ok = written == 37 && fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (!ok) {
        fprintf(stderr, "Unable to write %s.\n", IDENTITY_FILE);
        unlinkat(dir, IDENTITY_FILE, 0);
    }
    close(dir);
    *persistent = ok;
    return ok;
}

static bool parse_version(const char* s, uint8_t* major, uint8_t* minor, uint8_t* patch) {
    unsigned m, n, p;
    char extra;
    if (sscanf(s, "%u.%u.%u%c", &m, &n, &p, &extra) != 3 ||
        m > 255 || n > 255 || p > 255) return false;
    *major = (uint8_t)m; *minor = (uint8_t)n; *patch = (uint8_t)p;
    return true;
}

static bool make_zip(const char* input_dir, const char* zip_path) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        if (chdir(input_dir) != 0) {
            perror("chdir(game-dir)");
            _exit(127);
        }
        execlp("zip", "zip", "-q", "-r", zip_path, ".", "-x", IDENTITY_FILE, (char*)NULL);
        perror("exec zip");
        _exit(127);
    }
    int status;
    pid_t finished;
    do { finished = waitpid(pid, &status, 0); }
    while (finished == -1 && errno == EINTR);
    return finished == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool assemble_cartridge(const char* zip_path, const char* output_path,
                               const vcart_header_t* header) {
    FILE* src = fopen(zip_path, "rb");
    if (!src) { perror("open zip"); return false; }
    FILE* dst = fopen(output_path, "wb");
    if (!dst) { perror("open cartridge"); fclose(src); return false; }

    bool ok = vcart_write_header(dst, header) == VCART_OK;
    unsigned char buf[8192];
    size_t n;
    while (ok && (n = fread(buf, 1, sizeof(buf), src)) != 0) {
        if (fwrite(buf, 1, n, dst) != n) ok = false;
    }
    if (ferror(src)) ok = false;
    if (fclose(src) != 0) ok = false;
    if (fclose(dst) != 0) ok = false;
    if (!ok) {
        fprintf(stderr, "Error writing cartridge: %s\n", output_path);
        remove(output_path);
    }
    return ok;
}

int main(int argc, char* argv[]) {
    const char* input_dir = NULL;
    const char* output_file = NULL;
    const char* title = NULL;
    const char* uuid_string = NULL;
    bool verbose = false;
    bool init_identity = false;
    bool persistent = false;
    vcart_header_t header = {0};
    memcpy(header.magic, VCART_MAGIC, 4);
    header.header_version = VCART_STRUCT_VERSION;
    header.min_os_major = VCART_OS_MAJOR;
    header.min_os_minor = VCART_OS_MINOR;
    header.min_os_patch = VCART_OS_PATCH;

    for (int i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "-i") || !strcmp(argv[i], "--input")) && i + 1 < argc)
            input_dir = argv[++i];
        else if ((!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) && i + 1 < argc)
            output_file = argv[++i];
        else if ((!strcmp(argv[i], "-t") || !strcmp(argv[i], "--title")) && i + 1 < argc)
            title = argv[++i];
        else if (!strcmp(argv[i], "--init-identity"))
            init_identity = true;
        else if (!strcmp(argv[i], "--uuid") && i + 1 < argc)
            uuid_string = argv[++i];
        else if (!strcmp(argv[i], "--os") && i + 1 < argc) {
            if (!parse_version(argv[++i], &header.min_os_major,
                               &header.min_os_minor, &header.min_os_patch)) {
                fprintf(stderr, "Invalid --os version (expected 0..255.0..255.0..255)\n");
                return 1;
            }
        } else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
            verbose = true;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]); return 0;
        } else {
            fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
            usage(argv[0]); return 1;
        }
    }
    if (!input_dir || (init_identity ? (output_file || title) :
        (!output_file || !title || strlen(title) > 31 || !*title))) {
        fprintf(stderr, "Packaging requires -i, -o, and a title of 1..31 bytes.\n"
                        "Initialization requires -i and --init-identity, without -o or -t.\n");
        usage(argv[0]);
        return 1;
    }
    struct stat st;
    if (stat(input_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Game directory is missing or invalid: %s\n", input_dir);
        return 1;
    }
    if (!resolve_identity(input_dir, uuid_string, init_identity,
                          header.cart_uuid, &persistent)) return 1;
    if (init_identity) {
        char text[37];
        vcart_uuid_to_string(header.cart_uuid, text);
        printf("Created %s/%s: %s\nCommit this file with your game source.\n",
               input_dir, IDENTITY_FILE, text);
        return 0;
    }
    if (!persistent && !uuid_string)
        fprintf(stderr, "WARNING: uninitialized game; generating a build-only UUID.\n"
                "Initialize new games before packaging; adopt the printed UUID for existing saves.\n");
    memcpy(header.title, title, strlen(title));

    char tmpdir[] = "/tmp/sbee-pack-XXXXXX";
    if (!mkdtemp(tmpdir)) { perror("mkdtemp"); return 1; }
    char zip_path[sizeof(tmpdir) + 16];
    snprintf(zip_path, sizeof(zip_path), "%s/payload.zip", tmpdir);

    if (verbose) fprintf(stderr, "[sbee-pack] Compressing %s\n", input_dir);
    bool ok = make_zip(input_dir, zip_path);
    if (ok && stat(zip_path, &st) != 0) ok = false;
    if (ok && (st.st_size <= 0 || (uint64_t)st.st_size > UINT32_MAX)) {
        fprintf(stderr, "ZIP payload must be between 1 byte and 4 GiB.\n");
        ok = false;
    }
    if (ok) {
        header.payload_size = (uint32_t)st.st_size;
        ok = assemble_cartridge(zip_path, output_file, &header);
    }
    remove(zip_path);
    rmdir(tmpdir);
    if (!ok) {
        fprintf(stderr, "Cartridge packaging failed (is the 'zip' command installed?).\n");
        return 1;
    }
    char uuid_text[37];
    vcart_uuid_to_string(header.cart_uuid, uuid_text);
    printf("Wrote %s (%u ZIP bytes)\nGame UUID: %s\n", output_file,
           header.payload_size, uuid_text);
    if (!persistent && !uuid_string) printf("Keep this UUID and pass --uuid on subsequent builds to preserve saves.\n");
    return 0;
}
