/* Pixel-level texture smoke test; runs with SDL_VIDEODRIVER=dummy. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Texture test failed at line %d: %s (SDL: %s)\n", \
            __LINE__, #expr, SDL_GetError()); \
    return 1; \
} } while (0)

static bool pixel_rgb(SDL_Renderer *renderer, int x, int y,
                      uint8_t r, uint8_t g, uint8_t b) {
    SDL_Rect rect = {x, y, 1, 1};
    uint8_t pixel[4] = {0};
    if (SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGBA32,
                             pixel, (int)sizeof(pixel)) != 0) return false;
    return pixel[0] == r && pixel[1] == g && pixel[2] == b;
}

int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
    sb_graphics_t *gfx = sb_graphics_create("SugarBee texture test", 64, 48);
    CHECK(gfx != NULL);
    /* Red, green, blue, fully transparent white (in RGBA byte order). */
    const uint8_t pixels[] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 255, 0
    };
    CHECK(sb_graphics_texture_rgba(gfx, 2, 2, pixels, sizeof(pixels) - 1) ==
          SB_TEXTURE_INVALID);
    CHECK(sb_graphics_texture_rgba(gfx, 0, 2, pixels, sizeof(pixels)) ==
          SB_TEXTURE_INVALID);
    CHECK(sb_graphics_texture_rgba(gfx, 2, 2, NULL, sizeof(pixels)) ==
          SB_TEXTURE_INVALID);
    sb_texture_t first = sb_graphics_texture_rgba(gfx, 2, 2, pixels, sizeof(pixels));
    CHECK(first != SB_TEXTURE_INVALID);
    sb_graphics_clear(gfx, 8, 12, 32);
    CHECK(sb_graphics_sprite(gfx, first, 5, 7));

    /* This test runs in its own process under SDL_VIDEODRIVER=dummy.
     * The one window created above has the first SDL window ID (1).
     * Readback checks RGBA upload and alpha blending at actual pixels.
     */
    SDL_Window *window = SDL_GetWindowFromID(1);
    CHECK(window != NULL);
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    CHECK(renderer != NULL);
    const SDL_Rect rect = { 5, 7, 2, 2 };
    uint8_t out[16] = {0};
    CHECK(SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGBA32, out, 8) == 0);
    CHECK(out[0] == 255 && out[1] == 0 && out[2] == 0);
    CHECK(out[4] == 0 && out[5] == 255 && out[6] == 0);
    CHECK(out[8] == 0 && out[9] == 0 && out[10] == 255);
    CHECK(out[12] == 8 && out[13] == 12 && out[14] == 32);

    /* Crop the top row (red/green), enlarge it 2x, then flip horizontally.
     * Pixel checks exercise actual nearest-neighbor output and orientation. */
    CHECK(sb_graphics_sprite_region(gfx, first, 12, 2,
                                    0, 0, 2, 1, 4, 2, true, false));
    CHECK(pixel_rgb(renderer, 12, 2, 0, 255, 0));
    CHECK(pixel_rgb(renderer, 13, 3, 0, 255, 0));
    CHECK(pixel_rgb(renderer, 14, 2, 255, 0, 0));
    CHECK(pixel_rgb(renderer, 15, 3, 255, 0, 0));

    /* Vertical flip of the whole 2x2 image: blue and transparent on top,
     * red and green below. Confirm the transparent corner blends to clear. */
    CHECK(sb_graphics_sprite_region(gfx, first, 20, 3,
                                    0, 0, 2, 2, 2, 2, false, true));
    CHECK(pixel_rgb(renderer, 20, 3, 0, 0, 255));
    CHECK(pixel_rgb(renderer, 21, 3, 8, 12, 32));
    CHECK(pixel_rgb(renderer, 20, 4, 255, 0, 0));
    CHECK(pixel_rgb(renderer, 21, 4, 0, 255, 0));

    /* Valid 1x1 source; reject every out-of-bounds and zero-area case.
     * The subtraction-based implementation also avoids integer overflow. */
    CHECK(sb_graphics_sprite_region(gfx, first, 24, 3,
                                    1, 0, 1, 1, 3, 4, false, false));
    CHECK(pixel_rgb(renderer, 26, 6, 0, 255, 0));
    CHECK(!sb_graphics_sprite_region(gfx, first, 0, 0,
                                     -1, 0, 1, 1, 1, 1, false, false));
    CHECK(!sb_graphics_sprite_region(gfx, first, 0, 0,
                                     0, -1, 1, 1, 1, 1, false, false));
    CHECK(!sb_graphics_sprite_region(gfx, first, 0, 0,
                                     1, 0, 2, 1, 2, 1, false, false));
    CHECK(!sb_graphics_sprite_region(gfx, first, 0, 0,
                                     0, 1, 1, 2, 1, 2, false, false));
    CHECK(!sb_graphics_sprite_region(gfx, first, 0, 0,
                                     0, 0, 0, 1, 1, 1, false, false));
    CHECK(!sb_graphics_sprite_region(gfx, first, 0, 0,
                                     0, 0, 1, 1, 0, 1, false, false));
    CHECK(!sb_graphics_sprite_region(gfx, first, 0, 0,
                                     0, 0, 1, 1, 1, -1, false, false));
    CHECK(!sb_graphics_sprite_region(gfx, first, 0, 0,
                                     INT_MAX, 0, 1, 1, 1, 1, false, false));
    CHECK(sb_graphics_texture_destroy(gfx, first));
    CHECK(!sb_graphics_sprite_region(gfx, first, 0, 0,
                                     0, 0, 1, 1, 1, 1, false, false));
    CHECK(!sb_graphics_sprite(gfx, first, 5, 7));
    CHECK(!sb_graphics_texture_destroy(gfx, first));
    sb_texture_t next = sb_graphics_texture_rgba(gfx, 2, 2, pixels, sizeof(pixels));
    CHECK(next != SB_TEXTURE_INVALID && next != first);
    CHECK(!sb_graphics_sprite(gfx, first, 0, 0));
    CHECK(sb_graphics_sprite(gfx, next, 10, 10));
    sb_graphics_present(gfx);
    /* Intentionally leave next alive to test graphics-context cleanup. */
    sb_graphics_destroy(gfx);
    SDL_Quit();
    return 0;
}
