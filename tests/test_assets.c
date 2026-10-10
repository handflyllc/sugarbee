#include "sb_assets.h"
#include "sb_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "Asset test failed at %d: %s\n", __LINE__, #cond); \
    return 1; } } while (0)

int main(int argc, char **argv) {
    CHECK(argc == 2);
    const char *cart = argv[1];
    uint8_t *data = NULL;
    size_t len = 0;
    CHECK(sb_asset_read(cart, "main.lua", &data, &len) == SB_ASSET_OK);
    CHECK(len > 10 && memcmp(data, "-- Milestone", 12) == 0);
    free(data);
    CHECK(sb_asset_read(cart, "assets/bee.png", &data, &len) == SB_ASSET_OK);
    CHECK(len > 20);
    uint8_t *pixels = NULL;
    int w = 0, h = 0;
    size_t pixels_len = 0;
    CHECK(sb_png_decode_rgba(data, len, &pixels, &w, &h, &pixels_len));
    CHECK(w == 48 && h == 32);
    free(pixels);
    free(data);
    CHECK(sb_asset_read(cart, "assets/missing.png", &data, &len) == SB_ASSET_NOT_FOUND);
    CHECK(sb_asset_read(cart, "../etc/passwd", &data, &len) == SB_ASSET_INVALID_PATH);
    CHECK(sb_asset_read(cart, "/main.lua", &data, &len) == SB_ASSET_INVALID_PATH);
    CHECK(sb_asset_read(cart, "assets\\bee.png", &data, &len) == SB_ASSET_INVALID_PATH);
    CHECK(sb_asset_read(cart, "assets//bee.png", &data, &len) == SB_ASSET_INVALID_PATH);
    CHECK(sb_asset_read(cart, "./main.lua", &data, &len) == SB_ASSET_INVALID_PATH);
    CHECK(sb_asset_read("/nonexistent/sugarbee.vcart", "main.lua", &data, &len) == SB_ASSET_IO_ERROR);
    return 0;
}
