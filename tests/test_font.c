/* Font orientation, lowercase, punctuation, scaling and fallback regression. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Font check failed: %s\n", #x); return 1; } } while (0)

static int lit(SDL_Renderer *renderer, int x, int y) {
    SDL_Rect rect = {x, y, 1, 1};
    uint8_t p[4];
    if (SDL_RenderReadPixels(renderer, &rect, SDL_PIXELFORMAT_RGBA32, p, 4)) return -1;
    return p[0] == 255 && p[1] == 255 && p[2] == 255;
}

int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
    sb_graphics_t *g = sb_graphics_create("Font test", 64, 48);
    CHECK(g != NULL);
    SDL_Window *window = SDL_GetWindowFromID(1);
    CHECK(window != NULL);
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    CHECK(renderer != NULL);
    sb_graphics_clear(g, 0, 0, 0);
    sb_graphics_text(g, "Fa{?", 0, 0, 255, 255, 255, 1);
    /* F's vertical stem is on the left, not mirrored to the right. */
    CHECK(lit(renderer, 1, 5) == 1);
    CHECK(lit(renderer, 6, 5) == 0);
    /* Lowercase a starts below the cap line. */
    CHECK(lit(renderer, 10, 0) == 0);
    CHECK(lit(renderer, 10, 2) == 1);
    /* A brace has its own shape, rather than the old question-mark fallback. */
    CHECK(lit(renderer, 20, 0) == 1);
    CHECK(lit(renderer, 17, 0) == 0);
    sb_graphics_text(g, "\001", 32, 0, 255, 255, 255, 1);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            CHECK(lit(renderer, 24 + x, y) == lit(renderer, 32 + x, y));
    sb_graphics_text(g, "F\nF", 0, 12, 255, 255, 255, 2);
    CHECK(lit(renderer, 2, 22) == 1);
    CHECK(lit(renderer, 3, 23) == 1);
    CHECK(lit(renderer, 12, 22) == 0);
    CHECK(lit(renderer, 2, 42) == 1);
    sb_graphics_destroy(g);
    SDL_Quit();
    return 0;
}
