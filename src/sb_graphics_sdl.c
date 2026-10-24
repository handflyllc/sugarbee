/* SDL2 backend for SugarBee's language-neutral 2D graphics interface. */
#include "sb_graphics.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

extern const unsigned char font8x8_basic[128][8];

typedef struct {
    SDL_Texture *texture;
    uint16_t generation;
    int width;
    int height;
} texture_slot_t;

struct sb_graphics {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *game_frame;
    int width;
    int height;
    texture_slot_t *slots;
    size_t slot_count;
    size_t slot_capacity;
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

    if (!SDL_RenderTargetSupported(graphics->renderer)) {
        SDL_Log("SugarBee: renderer does not support render targets");
        goto error;
    }

    graphics->game_frame = SDL_CreateTexture(
        graphics->renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        width, height);

    if (!graphics->game_frame) {
        SDL_Log("SugarBee: could not create game frame: %s",
        SDL_GetError());
        goto error;
    }

    if (SDL_SetTextureBlendMode(graphics->game_frame, SDL_BLENDMODE_NONE) != 0 ||
        SDL_SetTextureScaleMode(graphics->game_frame, SDL_ScaleModeNearest) != 0) {
            SDL_Log("SugarBee: could not configure game frame: %s", SDL_GetError());
          goto error;
        }

        /* Start with a defined image, even if paused before the first draw. */
        if (!sb_graphics_begin_game_frame(graphics) ||
            SDL_SetRenderTarget(graphics->renderer, NULL) != 0) {
                SDL_Log("SugarBee: could not initialize game frame: %s", SDL_GetError());
                goto error;
            }

    return graphics;

error:
    sb_graphics_destroy(graphics);
    return NULL;
}

void sb_graphics_destroy(sb_graphics_t *graphics) {
    if (!graphics) return;
    for (size_t i = 0; i < graphics->slot_count; ++i) {
        if (graphics->slots[i].texture)
            SDL_DestroyTexture(graphics->slots[i].texture);
    }
    free(graphics->slots);
    if (graphics->game_frame) SDL_DestroyTexture(graphics->game_frame);
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

bool sb_graphics_begin_game_frame(sb_graphics_t *graphics) {
    if (!graphics || !graphics->game_frame) return false;

    if (SDL_SetRenderTarget(graphics->renderer, graphics->game_frame) != 0) {
        return false;
    }

    if (SDL_SetRenderDrawColor(graphics->renderer, 0, 0, 0, 255) != 0) {
        return false;
    }

    return SDL_RenderClear(graphics->renderer) == 0;
}

bool sb_graphics_show_game_frame(sb_graphics_t *graphics) {
    if (!graphics || !graphics->game_frame) return false;

    if (SDL_SetRenderTarget(graphics->renderer, NULL) != 0) {
        return false;
    }

    if (SDL_SetRenderDrawColor(graphics->renderer, 0, 0, 0, 255) != 0 ||
        SDL_RenderClear(graphics->renderer) != 0) {
            return false;
        }

        return SDL_RenderCopy(graphics->renderer, graphics->game_frame, NULL, NULL) == 0;
}

void sb_graphics_present(sb_graphics_t *graphics) {
    if (graphics) SDL_RenderPresent(graphics->renderer);
}

/* Handle encoding: high 16 bits = generation; low 16 bits = slot + 1.
 * Lookup during drawing is constant time. A destroyed handle remains invalid.
 * A slot is retired rather than wrapping its generation after 65535 releases.
 */
#define SB_TEXTURE_SLOT_MASK 0xffffu
#define SB_TEXTURE_MAX_SLOTS 65535u

static texture_slot_t *find_texture(sb_graphics_t *graphics, sb_texture_t handle) {
    if (!graphics || handle == SB_TEXTURE_INVALID) return NULL;
    const uint32_t slot_plus_one = handle & SB_TEXTURE_SLOT_MASK;
    const uint32_t generation = handle >> 16;
    if (!slot_plus_one || !generation ||
        (size_t)(slot_plus_one - 1) >= graphics->slot_count) return NULL;
    texture_slot_t *slot = &graphics->slots[slot_plus_one - 1];
    return slot->texture && slot->generation == generation ? slot : NULL;
}

static texture_slot_t *allocate_slot(sb_graphics_t *graphics, size_t *index) {
    for (size_t i = 0; i < graphics->slot_count; ++i) {
        if (!graphics->slots[i].texture && graphics->slots[i].generation != 0) {
            *index = i;
            return &graphics->slots[i];
        }
    }
    if (graphics->slot_count == SB_TEXTURE_MAX_SLOTS) return NULL;
    if (graphics->slot_count == graphics->slot_capacity) {
        size_t new_capacity = graphics->slot_capacity ? graphics->slot_capacity * 2 : 8;
        if (new_capacity < graphics->slot_capacity ||
            new_capacity > SB_TEXTURE_MAX_SLOTS) new_capacity = SB_TEXTURE_MAX_SLOTS;
        if (new_capacity > SIZE_MAX / sizeof(*graphics->slots)) return NULL;
        texture_slot_t *slots = realloc(graphics->slots,
                                        new_capacity * sizeof(*slots));
        if (!slots) return NULL;
        memset(slots + graphics->slot_capacity, 0,
               (new_capacity - graphics->slot_capacity) * sizeof(*slots));
        graphics->slots = slots;
        graphics->slot_capacity = new_capacity;
    }
    *index = graphics->slot_count++;
    graphics->slots[*index].generation = 1;
    return &graphics->slots[*index];
}

sb_texture_t sb_graphics_texture_rgba(sb_graphics_t *graphics,
                                      int width, int height,
                                      const uint8_t *rgba, size_t length) {
    if (!graphics || !rgba || width <= 0 || height <= 0 ||
        width > INT_MAX / 4) return SB_TEXTURE_INVALID;
    /* Avoid overflows before multiplying dimensions or passing an SDL pitch. */
    const size_t row_bytes = (size_t)width * 4;
    if ((size_t)height > SIZE_MAX / row_bytes ||
        length != row_bytes * (size_t)height) return SB_TEXTURE_INVALID;

    SDL_Texture *texture = SDL_CreateTexture(graphics->renderer,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC,
                                             width, height);
    if (!texture) return SB_TEXTURE_INVALID;
    /* Independent of the output/window scaling policy: enlarged sprite
     * sheet pixels stay crisp instead of receiving linear interpolation. */
    if (SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0 ||
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest) != 0 ||
        SDL_UpdateTexture(texture, NULL, rgba, (int)row_bytes) != 0) {
        SDL_DestroyTexture(texture);
        return SB_TEXTURE_INVALID;
    }
    size_t index = 0;
    texture_slot_t *slot = allocate_slot(graphics, &index);
    if (!slot) {
        SDL_DestroyTexture(texture);
        return SB_TEXTURE_INVALID;
    }
    slot->texture = texture;
    slot->width = width;
    slot->height = height;
    return ((uint32_t)slot->generation << 16) | (uint32_t)(index + 1);
}

bool sb_graphics_sprite_region(sb_graphics_t *graphics, sb_texture_t texture,
                               int x, int y,
                               int src_x, int src_y, int src_width, int src_height,
                               int dest_width, int dest_height,
                               bool flip_x, bool flip_y) {
    texture_slot_t *slot = find_texture(graphics, texture);
    if (!slot || src_x < 0 || src_y < 0 || src_width <= 0 || src_height <= 0 ||
        dest_width <= 0 || dest_height <= 0 ||
        src_width > slot->width || src_height > slot->height ||
        src_x > slot->width - src_width ||
        src_y > slot->height - src_height) return false;

    SDL_Rect source = { src_x, src_y, src_width, src_height };
    SDL_Rect destination = { x, y, dest_width, dest_height };
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (flip_x) flip = (SDL_RendererFlip)(flip | SDL_FLIP_HORIZONTAL);
    if (flip_y) flip = (SDL_RendererFlip)(flip | SDL_FLIP_VERTICAL);
    return SDL_RenderCopyEx(graphics->renderer, slot->texture, &source,
                            &destination, 0.0, NULL, flip) == 0;
}

bool sb_graphics_sprite(sb_graphics_t *graphics, sb_texture_t texture,
                        int x, int y) {
    texture_slot_t *slot = find_texture(graphics, texture);
    return slot && sb_graphics_sprite_region(graphics, texture, x, y,
                                              0, 0, slot->width, slot->height,
                                              slot->width, slot->height,
                                              false, false);
}

bool sb_graphics_texture_destroy(sb_graphics_t *graphics, sb_texture_t texture) {
    texture_slot_t *slot = find_texture(graphics, texture);
    if (!slot) return false;
    SDL_DestroyTexture(slot->texture);
    slot->texture = NULL;
    slot->width = 0;
    slot->height = 0;
    /* Generation 0 means retired; never reuse a slot after wraparound. */
    slot->generation++;
    return true;
}

void sb_graphics_clear_cartridge_textures(sb_graphics_t *graphics) {
    if (!graphics) return;

    for (size_t i = 0; i < graphics->slot_count; ++i) {
        texture_slot_t *slot = &graphics->slots[i];
        if (!slot->texture) continue;

        sb_texture_t handle = ((uint32_t)slot->generation << 16) | (uint32_t)(i + 1);

        sb_graphics_texture_destroy(graphics, handle);
    }
}
