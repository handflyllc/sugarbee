#include "vcart.h"

#include <stdio.h>
#include <string.h>

static int failed = 0;

static vcart_header_t valid_header(void) {
    vcart_header_t h = {0};
    memcpy(h.magic, VCART_MAGIC, 4);
    h.header_version = VCART_STRUCT_VERSION;
    h.min_os_major = VCART_OS_MAJOR;
    h.min_os_minor = VCART_OS_MINOR;
    h.min_os_patch = VCART_OS_PATCH;
    h.payload_size = 4;
    memcpy(h.title, "Test Game", sizeof("Test Game"));
    return h;
}

static void check_case(const char* name, vcart_header_t h, size_t payload_bytes,
                       vcart_err_t expected) {
    FILE* fp = tmpfile();
    if (!fp) { fprintf(stderr, "tmpfile failed\n"); failed++; return; }
    const unsigned char payload[4] = { 'P', 'K', 3, 4 };
    if (vcart_write_header(fp, &h) != VCART_OK ||
        fwrite(payload, 1, payload_bytes, fp) != payload_bytes) {
        fprintf(stderr, "write failed\n"); failed++;
        fclose(fp);
        return;
    }
    vcart_header_t parsed = {0};
    vcart_err_t actual = vcart_parse_header(fp, &parsed);
    if (actual != expected) {
        fprintf(stderr, "%s: got %s; expected %s\n", name,
                vcart_error_string(actual), vcart_error_string(expected));
        failed++;
    }
    fclose(fp);
}

int main(void) {
    if (sizeof(vcart_header_t) != VCART_HEADER_SIZE) {
        fprintf(stderr, "wrong packed header size\n");
        return 1;
    }
    vcart_header_t h = valid_header();
    check_case("valid", h, 4, VCART_OK);
    h.min_os_patch = (uint8_t)(VCART_OS_PATCH + 1);
    check_case("future patch", h, 4, VCART_ERR_VERSION_MISMATCH);
    h = valid_header();
    h.min_os_minor = (uint8_t)(VCART_OS_MINOR + 1);
    check_case("future minor", h, 4, VCART_ERR_VERSION_MISMATCH);
    h = valid_header();
    h.min_os_major = (uint8_t)(VCART_OS_MAJOR + 1);
    check_case("future major", h, 4, VCART_ERR_VERSION_MISMATCH);
    h = valid_header();
    h.header_version++;
    check_case("future header", h, 4, VCART_ERR_UNSUPPORTED_HEADER_VERSION);
    h = valid_header();
    h.magic[0] = 'X';
    check_case("wrong magic", h, 4, VCART_ERR_INVALID_MAGIC);
    h = valid_header();
    check_case("truncated payload", h, 2, VCART_ERR_PAYLOAD_TRUNCATED);
    h = valid_header();
    memset(h.title, 'A', sizeof(h.title));
    check_case("unterminated title", h, 4, VCART_ERR_CORRUPT_HEADER);

    h = valid_header();
    h.min_os_major = 0;
    h.min_os_minor = 255;
    h.min_os_patch = 255;
    check_case("older major accepted", h, 4, VCART_OK);

    /* Verify on-disk little-endian encoding independent of host byte order. */
    h = valid_header();
    h.header_version = 0x1234;
    h.payload_size = 0x01020304u;
    FILE* fp = tmpfile();
    if (!fp || vcart_write_header(fp, &h) != VCART_OK || fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "unable to test header byte order\n");
        failed++;
    } else {
        unsigned char raw[VCART_HEADER_SIZE];
        if (fread(raw, 1, sizeof(raw), fp) != sizeof(raw) ||
            raw[4] != 0x34 || raw[5] != 0x12 ||
            raw[12] != 0x04 || raw[13] != 0x03 ||
            raw[14] != 0x02 || raw[15] != 0x01) {
            fprintf(stderr, "header is not encoded little-endian\n");
            failed++;
        }
    }
    if (fp) fclose(fp);

    /* A short file must fail before touching the remaining header fields. */
    fp = tmpfile();
    if (!fp) {
        failed++;
    } else {
        fwrite("SBEE", 1, 4, fp);
        vcart_header_t parsed = {0};
        if (vcart_parse_header(fp, &parsed) != VCART_ERR_CORRUPT_HEADER) {
            fprintf(stderr, "short header was accepted\n");
            failed++;
        }
        fclose(fp);
    }
    printf("vcart header: %s\n", failed ? "FAIL" : "PASS");
    return failed ? 1 : 0;
}
