/* Headless smoke test for the internal, language-neutral graphics services. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <stdio.h>

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL video initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    sb_graphics_t *graphics = sb_graphics_create("SugarBee graphics test", 320, 200);
    if (!graphics) {
        fprintf(stderr, "Graphics initialization failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (sb_graphics_width(graphics) != 320 || sb_graphics_height(graphics) != 200) {
        fprintf(stderr, "Unexpected logical dimensions\n");
        sb_graphics_destroy(graphics);
        SDL_Quit();
        return 1;
    }

    sb_graphics_clear(graphics, 8, 12, 32);
    sb_graphics_rect(graphics, 50, 50, 100, 80, 240, 180, 50, true);
    sb_graphics_rect(graphics, 10, 10, 30, 20, 255, 0, 0, false);
    sb_graphics_text(graphics, "SUGARBEE\nOK", 60, 150, 255, 255, 255, 1);
    sb_graphics_present(graphics);

    sb_graphics_destroy(graphics);
    SDL_Quit();
    return 0;
}
