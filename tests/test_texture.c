/* Pixel-level texture smoke test; runs with SDL_VIDEODRIVER=dummy. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Texture test failed at line %d: %s (SDL: %s)\n", \
            __LINE__, #expr, SDL_GetError()); \
    return 1; \
} } while (0)

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
    CHECK(sb_graphics_texture_destroy(gfx, first));
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
