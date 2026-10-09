/*
 * PROJECT SUGARBEE - Virtual Cartridge Parser Implementation
 */

#include "vcart.h"
#include <string.h>

vcart_err_t vcart_parse_header(FILE* fp, vcart_header_t* out_header) {
    if (!fp || !out_header) {
        return VCART_ERR_FILE_READ;
    }

    // Seek to start of file
    fseek(fp, 0, SEEK_SET);

    // Read 64-byte binary header
    size_t bytes_read = fread(out_header, 1, sizeof(vcart_header_t), fp);
    if (bytes_read != sizeof(vcart_header_t)) {
        return VCART_ERR_CORRUPT_HEADER;
    }

    // Verify Magic Bytes ("SBEE")
    if (memcmp(out_header->magic, VCART_MAGIC, 4) != 0) {
        return VCART_ERR_INVALID_MAGIC;
    }

    // Verify OS Version Requirements
    if (out_header->min_os_major > VCART_OS_MAJOR) {
        return VCART_ERR_VERSION_MISMATCH;
    }

    // Check payload size against file bounds
    fseek(fp, 0, SEEK_END);
    long total_file_size = ftell(fp);
    if (total_file_size < (long)(sizeof(vcart_header_t) + out_header->payload_size)) {
        return VCART_ERR_PAYLOAD_TRUNCATED;
    }

    return VCART_OK;
}

const char* vcart_error_string(vcart_err_t err) {
    switch (err) {
        case VCART_OK: return "OK";
        case VCART_ERR_FILE_READ: return "File read error";
        case VCART_ERR_INVALID_MAGIC: return "Invalid magic bytes (Expected 'SBEE')";
        case VCART_ERR_VERSION_MISMATCH: return "Cartridge requires a newer SugarBee OS version";
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