/* SDL2 backend for SugarBee's language-neutral 2D graphics interface. */
#include "sb_graphics.h"

#include <SDL2/SDL.h>
#include <stdlib.h>

extern const unsigned char font8x8_basic[128][8];

struct sb_graphics {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width;
    int height;
};

sb_graphics_t *sb_graphics_create(const char *title, int width, int height) {
    if (!title || width <= 0 || height <= 0) return NULL;

    sb_graphics_t *graphics = calloc(1, sizeof(*graphics));
    if (!graphics) return NULL;

    graphics->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, width, height,
                                        SDL_WINDOW_SHOWN);
    if (!graphics->window) goto error;

    graphics->renderer = SDL_CreateRenderer(graphics->window, -1,
                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    /* Software rendering lets headless SDL dummy-driver smoke tests run too. */
    if (!graphics->renderer) {
        graphics->renderer = SDL_CreateRenderer(graphics->window, -1,
                                                SDL_RENDERER_SOFTWARE);
    }
    if (!graphics->renderer) goto error;

    if (SDL_RenderSetLogicalSize(graphics->renderer, width, height) < 0) goto error;
    graphics->width = width;
    graphics->height = height;
    return graphics;

error:
    sb_graphics_destroy(graphics);
    return NULL;
}

void sb_graphics_destroy(sb_graphics_t *graphics) {
    if (!graphics) return;
    if (graphics->renderer) SDL_DestroyRenderer(graphics->renderer);
    if (graphics->window) SDL_DestroyWindow(graphics->window);
    free(graphics);
}

int sb_graphics_width(const sb_graphics_t *graphics) {
    return graphics ? graphics->width : 0;
}

int sb_graphics_height(const sb_graphics_t *graphics) {
    return graphics ? graphics->height : 0;
}

void sb_graphics_clear(sb_graphics_t *graphics, uint8_t r, uint8_t g, uint8_t b) {
    if (!graphics) return;
    SDL_SetRenderDrawColor(graphics->renderer, r, g, b, 255);
    SDL_RenderClear(graphics->renderer);
}

void sb_graphics_rect(sb_graphics_t *graphics, int x, int y, int width,
                      int height, uint8_t r, uint8_t g, uint8_t b, bool filled) {
    if (!graphics) return;
    SDL_Rect rect = { x, y, width, height };
    SDL_SetRenderDrawColor(graphics->renderer, r, g, b, 255);
    if (filled) SDL_RenderFillRect(graphics->renderer, &rect);
    else SDL_RenderDrawRect(graphics->renderer, &rect);
}

static void draw_char(sb_graphics_t *graphics, unsigned char c, int x, int y,
                      int scale) {
    if (c < 32 || c > 126) c = '?';
    const unsigned char *glyph = font8x8_basic[c];

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (glyph[row] & (1u << (7 - col))) {
                SDL_Rect pixel = {x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(graphics->renderer, &pixel);
            }
        }
    }
}

void sb_graphics_text(sb_graphics_t *graphics, const char *text, int x, int y,
                      uint8_t r, uint8_t g, uint8_t b, int scale) {
    if (!graphics || !text || scale <= 0) return;
    int start_x = x;
    SDL_SetRenderDrawColor(graphics->renderer, r, g, b, 255);
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '\n') {
            y += 10 * scale;
            x = start_x;
        } else {
            draw_char(graphics, *p, x, y, scale);
            x += 8 * scale;
        }
    }
}

void sb_graphics_present(sb_graphics_t *graphics) {
    if (graphics) SDL_RenderPresent(graphics->renderer);
}
