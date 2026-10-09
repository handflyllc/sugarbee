/*
 * PROJECT SUGARBEE - Cartridge Packager Utility (sbee-pack)
 * Usage: sbee-pack -i <source_dir> -o <output.vcart> -t "Game Title" [options]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>

#if defined(_WIN32)
  #include <direct.h>
  #define GetCurrentDir _getcwd
#else
  #include <unistd.h>
  #define GetCurrentDir getcwd
#endif

#include "vcart.h"

#define TOOL_VERSION "1.0.0"
#define TEMP_ZIP_FILE "sbee_temp_payload.zip"

// CLI Configuration Flags
typedef struct {
    char input_dir[256];
    char output_file[256];
    char title[32];
    uint8_t target_os_major;
    uint8_t target_os_minor;
    uint8_t target_os_patch;
    uint8_t hw_flags[3];
    bool verbose;
} pack_config_t;

// Standard Pseudo-UUID Generator for Cartridge Identification
void generate_uuid(uint8_t uuid[16]) {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < 16; i++) {
        uuid[i] = (uint8_t)(rand() % 256);
    }
    // Set RFC 4122 variant & version 4 bits
    uuid[6] = (uuid[6] & 0x0F) | 0x40; // Version 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80; // Variant 10xx
}

void print_usage(const char* prog_name) {
    printf("Project SugarBee Cartridge Packager v%s\n", TOOL_VERSION);
    printf("Usage: %s -i <input_dir> -o <output.vcart> -t \"Game Title\" [options]\n\n", prog_name);
    printf("Required Arguments:\n");
    printf("  -i, --input <dir>        Directory containing game assets and main.lua\n");
    printf("  -o, --output <file>      Target .vcart output file path\n");
    printf("  -t, --title <name>       Game title (max 31 chars)\n\n");
    printf("Optional Arguments:\n");
    printf("  --os <major.minor.patch> Minimum SugarBee OS required (default: %d.%d.%d)\n",
           VCART_OS_MAJOR, VCART_OS_MINOR, VCART_OS_PATCH);
    printf("  -v, --verbose            Enable verbose build logging\n");
    printf("  -h, --help               Display this help dialog\n");
}

long get_file_size(const char* filepath) {
    struct stat st;
    if (stat(filepath, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

bool create_zip_payload(const char* input_dir, const char* zip_output) {
    char command[1024];
    
    // Uses system zip CLI utility available on Linux/macOS
#if defined(_WIN32)
    snprintf(command, sizeof(command), "powershell -Command \"Compress-Archive -Path '%s\\*' -DestinationPath '%s' -Force\"", input_dir, zip_output);
#else
    snprintf(command, sizeof(command), "cd \"%s\" && zip -q -r \"../%s\" .", input_dir, zip_output);
#endif

    int ret = system(command);
    return (ret == 0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    pack_config_t config = {0};
    config.target_os_major = VCART_OS_MAJOR;
    config.target_os_minor = VCART_OS_MINOR;
    config.target_os_patch = VCART_OS_PATCH;

    // Parse Command-Line Arguments
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0) && i + 1 < argc) {
            strncpy(config.input_dir, argv[++i], sizeof(config.input_dir) - 1);
        } else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i + 1 < argc) {
            strncpy(config.output_file, argv[++i], sizeof(config.output_file) - 1);
        } else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--title") == 0) && i + 1 < argc) {
            strncpy(config.title, argv[++i], sizeof(config.title) - 1);
        } else if (strcmp(argv[i], "--os") == 0 && i + 1 < argc) {
            int maj, min, pat;
            if (sscanf(argv[++i], "%d.%d.%d", &maj, &min, &pat) == 3) {
                config.target_os_major = (uint8_t)maj;
                config.target_os_minor = (uint8_t)min;
                config.target_os_patch = (uint8_t)pat;
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    // Validate Required Inputs
    if (strlen(config.input_dir) == 0 || strlen(config.output_file) == 0 || strlen(config.title) == 0) {
        fprintf(stderr, "Error: Missing required arguments (-i, -o, -t).\n\n");
        print_usage(argv[0]);
        return 1;
    }

    printf("[sbee-pack] Packing target directory: '%s'\n", config.input_dir);

    // Step 1: Compress Payload Directory to Temporary ZIP
    if (config.verbose) printf("[sbee-pack] Compressing payload directory into ZIP archive...\n");
    if (!create_zip_payload(config.input_dir, TEMP_ZIP_FILE)) {
        fprintf(stderr, "Error: Failed to compress directory '%s'. Ensure 'zip' CLI utility is installed.\n", config.input_dir);
        return 1;
    }

    long payload_size = get_file_size(TEMP_ZIP_FILE);
    if (payload_size <= 0) {
        fprintf(stderr, "Error: Generated payload ZIP is empty or unreadable.\n");
        remove(TEMP_ZIP_FILE);
        return 1;
    }

    // Step 2: Build 64-Byte Binary Header
    vcart_header_t header = {0};
    memcpy(header.magic, VCART_MAGIC, 4);
    header.header_version = VCART_STRUCT_VERSION;
    header.min_os_major = config.target_os_major;
    header.min_os_minor = config.target_os_minor;
    header.min_os_patch = config.target_os_patch;
    header.payload_size = (uint32_t)payload_size;
    
    strncpy(header.title, config.title, sizeof(header.title) - 1);
    generate_uuid(header.cart_uuid);

    // Step 3: Write Output .vcart File
    FILE* out_fp = fopen(config.output_file, "wb");
    if (!out_fp) {
        fprintf(stderr, "Error: Cannot open output file '%s' for writing.\n", config.output_file);
        remove(TEMP_ZIP_FILE);
        return 1;
    }

    FILE* zip_fp = fopen(TEMP_ZIP_FILE, "rb");
    if (!zip_fp) {
        fprintf(stderr, "Error: Cannot open temporary ZIP file for reading.\n");
        fclose(out_fp);
        remove(TEMP_ZIP_FILE);
        return 1;
    }

    // Write Header
    fwrite(&header, sizeof(vcart_header_t), 1, out_fp);

    // Stream Payload Bytes
    char buffer[8192];
    size_t bytes_read = 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), zip_fp)) > 0) {
        fwrite(buffer, 1, bytes_read, out_fp);
    }

    fclose(zip_fp);
    fclose(out_fp);

    // Clean up temporary files
    remove(TEMP_ZIP_FILE);

    // Print Packaging Summary
    printf("\n==================================================\n");
    printf(" SUCCESS: Cartridge compiled to '%s'\n", config.output_file);
    printf(" Title:            %s\n", header.title);
    printf(" Required OS:      v%d.%d.%d\n", header.min_os_major, header.min_os_minor, header.min_os_patch);
    printf(" Header Size:      64 Bytes\n");
    printf(" Payload Size:     %u Bytes (%.2f KB)\n", header.payload_size, header.payload_size / 1024.0);
    printf(" Cartridge UUID:   ");
    for (int i = 0; i < 16; i++) printf("%02x", header.cart_uuid[i]);
    printf("\n==================================================\n");

    return 0;
}