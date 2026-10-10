#include "sb_assets.h"
#include "vcart.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <minizip/unzip.h>

static bool safe_path(const char *path) {
    if (!path || !*path || path[0] == '/') return false;
    size_t n = 0;
    const char *segment = path;
    for (const unsigned char *p = (const unsigned char *)path; ; ++p) {
        const unsigned char c = *p;
        if (++n > 240) return false;
        if (c == '/' || c == '\0') {
            size_t len = (const char *)p - segment;
            if (!len || (len == 1 && segment[0] == '.') ||
                (len == 2 && segment[0] == '.' && segment[1] == '.'))
                return false;
            if (!c) return true;
            segment = (const char *)p + 1;
        } else if (c < 32 || c == 127 || c == '\\' || c == ':') {
            return false;
        }
    }
}

const char *sb_asset_error_string(sb_asset_result_t result) {
    switch (result) {
        case SB_ASSET_OK: return "OK";
        case SB_ASSET_INVALID_PATH: return "Invalid cartridge asset path";
        case SB_ASSET_INVALID_CARTRIDGE: return "Invalid or unsupported cartridge";
        case SB_ASSET_NOT_FOUND: return "Asset not found";
        case SB_ASSET_TOO_LARGE: return "Cartridge resource limit exceeded";
        case SB_ASSET_CORRUPT: return "Invalid or damaged ZIP payload";
        case SB_ASSET_IO_ERROR: return "Cartridge I/O error";
        default: return "Unknown asset error";
    }
}

sb_asset_result_t sb_asset_read(const char *cartridge, const char *path,
                                 uint8_t **data, size_t *length) {
    if (data) *data = NULL;
    if (length) *length = 0;
    if (!data || !length || !cartridge || !safe_path(path))
        return SB_ASSET_INVALID_PATH;

    FILE *fp = fopen(cartridge, "rb");
    if (!fp) return SB_ASSET_IO_ERROR;
    vcart_header_t header;
    vcart_err_t status = vcart_parse_header(fp, &header);
    fclose(fp);
    if (status != VCART_OK) return SB_ASSET_INVALID_CARTRIDGE;
    if (header.payload_size > SB_ASSET_MAX_ARCHIVE) return SB_ASSET_TOO_LARGE;

    /* minizip 1.x tolerates the 64-byte SBEE prefix on Linux. */
    unzFile zip = unzOpen(cartridge);
    if (!zip) return SB_ASSET_CORRUPT;

    sb_asset_result_t result = SB_ASSET_CORRUPT;
    uint8_t *buffer = NULL;
    bool current_open = false;
    size_t total_size = 0;
    unsigned count = 0;
    bool found = false;
    unz_file_info target_info = {0};
    int z = unzGoToFirstFile(zip);
    while (z == UNZ_OK) {
        unz_file_info info;
        char name[242];
        memset(name, 0, sizeof(name));
        if (unzGetCurrentFileInfo(zip, &info, name, sizeof(name), NULL, 0, NULL, 0) != UNZ_OK ||
            info.size_filename >= sizeof(name)) goto done;
        /* Reject embedded NULs rather than accepting a shortened member name. */
        if (strlen(name) != (size_t)info.size_filename) goto done;
        if (++count > SB_ASSET_MAX_FILES ||
            info.uncompressed_size > SB_ASSET_MAX_TOTAL - total_size) {
            result = SB_ASSET_TOO_LARGE;
            goto done;
        }
        total_size += info.uncompressed_size;
        /* Directory entries are allowed, but their paths must be safe too. */
        size_t name_len = strlen(name);
        bool directory = name_len && name[name_len - 1] == '/';
        if (directory) name[name_len - 1] = '\0';
        bool valid_name = safe_path(name);
        if (directory) name[name_len - 1] = '/';
        if (!valid_name) {
            result = SB_ASSET_CORRUPT;
            goto done;
        }
        if (strcmp(name, path) == 0) {
            if (found) goto done; /* ambiguous duplicates are rejected */
            found = true;
            target_info = info;
            if (info.uncompressed_size > SB_ASSET_MAX_ENTRY) {
                result = SB_ASSET_TOO_LARGE;
                goto done;
            }
        }
        z = unzGoToNextFile(zip);
    }
    if (z != UNZ_END_OF_LIST_OF_FILE) goto done;
    if (!found) { result = SB_ASSET_NOT_FOUND; goto done; }
    if (unzLocateFile(zip, path, 1) != UNZ_OK) goto done;
    if (target_info.flag & 1u) goto done; /* encrypted entry */
    if (unzOpenCurrentFile(zip) != UNZ_OK) goto done;
    current_open = true;
    size_t expected = (size_t)target_info.uncompressed_size;
    buffer = malloc(expected + 1);
    if (!buffer) { result = SB_ASSET_IO_ERROR; goto done; }

    size_t received = 0;
    while (received < expected) {
        int got = unzReadCurrentFile(zip, buffer + received,
                                     (unsigned)(expected - received));
        if (got <= 0) goto done;
        received += (size_t)got;
    }
    uint8_t extra;
    if (unzReadCurrentFile(zip, &extra, 1) != 0) goto done;
    /* Closing the member checks its CRC when the stream reached EOF. */
    if (unzCloseCurrentFile(zip) != UNZ_OK) { current_open = false; goto done; }
    current_open = false;
    buffer[expected] = 0; /* convenient terminator for script bytes */
    *data = buffer;
    *length = expected;
    buffer = NULL;
    result = SB_ASSET_OK;

done:
    if (current_open) unzCloseCurrentFile(zip);
    free(buffer);
    unzClose(zip);
    return result;
}
