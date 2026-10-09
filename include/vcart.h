/*
 * PROJECT SUGARBEE - Virtual Cartridge Format Header
 * Target Specification v1.0
 */

#ifndef VCART_H
#define VCART_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define VCART_MAGIC "SBEE"
#define VCART_HEADER_SIZE 64
#define VCART_STRUCT_VERSION 1

// Current Host Engine OS Version
#define VCART_OS_MAJOR 1
#define VCART_OS_MINOR 0
#define VCART_OS_PATCH 0

// Error Return Codes for Cartridge Header Parsing
typedef enum {
    VCART_OK = 0,
    VCART_ERR_FILE_READ,
    VCART_ERR_INVALID_MAGIC,
    VCART_ERR_VERSION_MISMATCH,
    VCART_ERR_CORRUPT_HEADER,
    VCART_ERR_PAYLOAD_TRUNCATED
} vcart_err_t;

// 64-Byte Binary Header Struct (Packed)
#pragma pack(push, 1)
typedef struct {
    char magic[4];             // 0x00: "SBEE" Magic Bytes
    uint16_t header_version;   // 0x04: Version of this header struct (1)
    uint8_t min_os_major;      // 0x06: Required OS Major
    uint8_t min_os_minor;      // 0x07: Required OS Minor
    uint8_t min_os_patch;      // 0x08: Required OS Patch
    uint8_t hw_flags[3];       // 0x09: Reserved hardware feature flags
    uint32_t payload_size;     // 0x0C: Size of ZIP payload in bytes
    uint8_t cart_uuid[16];     // 0x10: 128-bit RFC 4122 v4 UUID
    char title[32];            // 0x20: Null-terminated Game Title (ASCII)
} vcart_header_t;              // Total: 64 Bytes (0x40)
#pragma pack(pop)

// API Functions
vcart_err_t vcart_parse_header(FILE* fp, vcart_header_t* out_header);
const char* vcart_error_string(vcart_err_t err);
void vcart_uuid_to_string(const uint8_t uuid[16], char out_str[37]);

#endif // VCART_H