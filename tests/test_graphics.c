/* Headless smoke test for the internal, language-neutral graphics services. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <stdio.h>

/* Read one pixel from the current drawing destination. */
static bool pixel_matches(SDL_Renderer *renderer, int x, int y, Uint8 r, Uint8 g, Uint8 b) {
    SDL_Rect area = {x, y, 1, 1};
    Uint8 pixel[4];

    if (SDL_RenderReadPixels(renderer, &area, SDL_PIXELFORMAT_RGBA32, pixel, sizeof(pixel)) != 0) {
        fprintf(stderr, "Pixel read failed: %s\n", SDL_GetError());
        return false;
    }

    return pixel[0] == r && pixel[1] == g && pixel[2] == b;
}

static bool retained_frame_test(sb_graphics_t *graphics) {
    /* This standalone test creates one SDL window. */
    SDL_Window *window = SDL_GetWindowFromID(1);
    if (!window) return false;

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if (!renderer) return false;

    /* Before any game drawing, the retained image should be black.  */
    if (!sb_graphics_show_game_frame(graphics) ||
        !pixel_matches(renderer, 15, 15, 0, 0, 0)) {
            fprintf(stderr, "Initial game frame was not black\n");
            return false;
        }

    /* Create a red game frame with a green square.  */
    if (!sb_graphics_begin_game_frame(graphics)) return false;
    sb_graphics_clear(graphics, 200, 0, 0);
    sb_graphics_rect(graphics, 10, 10, 20, 20, 0, 200, 0, true);

    if (!sb_graphics_show_game_frame(graphics) ||
        !pixel_matches(renderer, 15, 15, 0, 200, 0) ||
        !pixel_matches(renderer, 50, 50, 200, 0, 0)) {
        fprintf(stderr, "Game frame was not copied correctly\n");
        return false;
        }

    /* Cover the square with a blue host overlay. */
    sb_graphics_rect(graphics, 10, 10, 20, 20, 0, 0, 200, true);

    if (!pixel_matches(renderer, 15, 15, 0, 0, 200)) {
        fprintf(stderr, "Host overlay was not drawn\n");
        return false;
    }

    sb_graphics_present(graphics);

    /* Reusing the saved frame must restore green, not blue. */
    for (int frame = 0; frame < 3; ++frame) {
        if (!sb_graphics_show_game_frame(graphics) ||
        !pixel_matches(renderer, 15, 15, 0, 200, 0) ||
        !pixel_matches(renderer, 50, 50, 200, 0, 0)) {
            fprintf(stderr, "Saved frame changed after presentation\n");
            return false;
        }

        sb_graphics_present(graphics);
    }

    /* Starting a fresh frame must erase the previous game image. */
    if (!sb_graphics_begin_game_frame(graphics) ||
        !sb_graphics_show_game_frame(graphics) ||
        !pixel_matches(renderer, 15, 15, 0, 0, 0) ||
        !pixel_matches(renderer, 50, 50, 0, 0, 0)) {
            fprintf(stderr, "New game frame was not cleared\n");
            return false;
        }

    return true;
}

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

    if (!retained_frame_test(graphics)) {
        fprintf(stderr, "Retained-frame regression failed\n");
        sb_graphics_destroy(graphics);
        SDL_Quit();
        return 1;
    }

    sb_graphics_destroy(graphics);
    SDL_Quit();
    return 0;
}
