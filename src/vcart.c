/*
 * Project SugarBee - Virtual Cartridge Parser Implementation
 * Current format: 64-byte SBEE v1 header, followed by ZIP payload.
 * Binary fields are explicitly encoded/decoded as little-endian.
 */

#include "vcart.h"
#include <string.h>
#include <stdint.h>

typedef char vcart_header_must_be_64_bytes[(sizeof(vcart_header_t) == VCART_HEADER_SIZE) ? 1 : -1];

static uint16_t read_le16(const unsigned char* p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static uint32_t read_le32(const unsigned char* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

vcart_err_t vcart_write_header(FILE* fp, const vcart_header_t* h) {
    if (!fp || !h) return VCART_ERR_FILE_READ;
    unsigned char raw[VCART_HEADER_SIZE] = {0};
    memcpy(raw, h->magic, 4);
    raw[4] = (unsigned char)(h->header_version & 0xFFu);
    raw[5] = (unsigned char)((h->header_version >> 8) & 0xFFu);
    raw[6] = h->min_os_major;
    raw[7] = h->min_os_minor;
    raw[8] = h->min_os_patch;
    memcpy(raw + 9, h->hw_flags, 3);
    raw[12] = (unsigned char)(h->payload_size & 0xFFu);
    raw[13] = (unsigned char)((h->payload_size >> 8) & 0xFFu);
    raw[14] = (unsigned char)((h->payload_size >> 16) & 0xFFu);
    raw[15] = (unsigned char)((h->payload_size >> 24) & 0xFFu);
    memcpy(raw + 16, h->cart_uuid, 16);
    memcpy(raw + 32, h->title, 32);
    return fwrite(raw, 1, sizeof(raw), fp) == sizeof(raw)
           ? VCART_OK : VCART_ERR_FILE_READ;
}

static bool version_satisfied(uint8_t major, uint8_t minor, uint8_t patch) {
    const unsigned installed[3] = { VCART_OS_MAJOR, VCART_OS_MINOR, VCART_OS_PATCH };
    const unsigned required[3] = { major, minor, patch };
    for (int i = 0; i < 3; i++) {
        if (installed[i] != required[i]) return installed[i] > required[i];
    }
    return true;
}

vcart_err_t vcart_parse_header(FILE* fp, vcart_header_t* out_header) {
    if (!fp || !out_header || fseek(fp, 0, SEEK_SET) != 0) {
        return VCART_ERR_FILE_READ;
    }

    unsigned char raw[VCART_HEADER_SIZE];
    if (fread(raw, 1, sizeof(raw), fp) != sizeof(raw)) {
        return ferror(fp) ? VCART_ERR_FILE_READ : VCART_ERR_CORRUPT_HEADER;
    }
    memset(out_header, 0, sizeof(*out_header));
    memcpy(out_header->magic, raw, 4);
    out_header->header_version = read_le16(raw + 4);
    out_header->min_os_major = raw[6];
    out_header->min_os_minor = raw[7];
    out_header->min_os_patch = raw[8];
    memcpy(out_header->hw_flags, raw + 9, 3);
    out_header->payload_size = read_le32(raw + 12);
    memcpy(out_header->cart_uuid, raw + 16, 16);
    memcpy(out_header->title, raw + 32, 32);

    if (memcmp(out_header->magic, VCART_MAGIC, 4) != 0) {
        return VCART_ERR_INVALID_MAGIC;
    }

    if (out_header->header_version != VCART_STRUCT_VERSION) {
        return VCART_ERR_UNSUPPORTED_HEADER_VERSION;
    }

    if (!version_satisfied(out_header->min_os_major,
                           out_header->min_os_minor,
                           out_header->min_os_patch)) {
        return VCART_ERR_VERSION_MISMATCH;
    }

    if (fseek(fp, 0, SEEK_END) != 0) return VCART_ERR_FILE_READ;
    long total_file_size = ftell(fp);
    if (total_file_size < 0) return VCART_ERR_FILE_READ;

    /* Avoid overflowing signed long or size_t when checking untrusted values. */
    uint64_t expected = (uint64_t)VCART_HEADER_SIZE + (uint64_t)out_header->payload_size;
    if ((uint64_t)total_file_size < expected) {
        return VCART_ERR_PAYLOAD_TRUNCATED;
    }

    /* printf("%s", title) in the host requires a terminated title. */
    if (!memchr(out_header->title, '\0', sizeof(out_header->title))) {
        return VCART_ERR_CORRUPT_HEADER;
    }

    return VCART_OK;
}

const char* vcart_error_string(vcart_err_t err) {
    switch (err) {
        case VCART_OK: return "OK";
        case VCART_ERR_FILE_READ: return "File read error";
        case VCART_ERR_INVALID_MAGIC: return "Invalid magic bytes (Expected 'SBEE')";
        case VCART_ERR_UNSUPPORTED_HEADER_VERSION: return "Unsupported cartridge header version";
        case VCART_ERR_VERSION_MISMATCH: return "Cartridge requires a newer SugarBee version";
        case VCART_ERR_CORRUPT_HEADER: return "Header is corrupt or undersized";
        case VCART_ERR_PAYLOAD_TRUNCATED: return "Cartridge payload data is truncated or missing";
        default: return "Unknown cartridge error";
    }
}

void vcart_uuid_to_string(const uint8_t uuid[16], char out_str[37]) {
    snprintf(out_str, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5],
        uuid[6], uuid[7],
        uuid[8], uuid[9],
        uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]
    );
}
