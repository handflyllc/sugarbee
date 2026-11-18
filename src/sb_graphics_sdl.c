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
    int camera_x;
    int camera_y;
    uint8_t tint_r, tint_g, tint_b, tint_a;
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

void sb_graphics_sprite_tint(sb_graphics_t *graphics, uint8_t r, uint8_t g,
                             uint8_t b, uint8_t a) {
    if (!graphics) return;
    graphics->tint_r = r;
    graphics->tint_g = g;
    graphics->tint_b = b;
    graphics->tint_a = a;
}

void sb_graphics_sprite_tint_reset(sb_graphics_t *graphics) {
    sb_graphics_sprite_tint(graphics, 255, 255, 255, 255);
}

void sb_graphics_camera(sb_graphics_t *graphics, int x, int y) {
    if (!graphics) return;
    graphics->camera_x = x;
    graphics->camera_y = y;
}

void sb_graphics_camera_reset(sb_graphics_t *graphics) {
    sb_graphics_camera(graphics, 0, 0);
}

/* Clip in wide screen coordinates before narrowing for SDL. Used for filled
 * rectangles and individual outline edges, so clipping never invents an edge. */
static void fill_screen_rect(sb_graphics_t *graphics, int64_t x, int64_t y,
                             int64_t width, int64_t height) {
    int64_t right = x + width, bottom = y + height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > graphics->width) right = graphics->width;
    if (bottom > graphics->height) bottom = graphics->height;
    if (x >= right || y >= bottom) return;
    SDL_Rect rect = {(int)x, (int)y, (int)(right-x), (int)(bottom-y)};
    SDL_RenderFillRect(graphics->renderer, &rect);
}

bool sb_graphics_clip_reset(sb_graphics_t *graphics) {
    return graphics && SDL_RenderSetClipRect(graphics->renderer, NULL) == 0;
}

bool sb_graphics_clip(sb_graphics_t *graphics, int x, int y, int width, int height) {
    if (!graphics || width < 0 || height < 0) return false;
    int64_t left=x, top=y, right=(int64_t)x+width, bottom=(int64_t)y+height;
    if (left < 0) left=0;
    if (top < 0) top=0;
    if (right > graphics->width) right=graphics->width;
    if (bottom > graphics->height) bottom=graphics->height;
    SDL_Rect clip={0,0,0,0};
    if (left < right && top < bottom) {
        clip=(SDL_Rect){(int)left,(int)top,(int)(right-left),(int)(bottom-top)};
    }
    return SDL_RenderSetClipRect(graphics->renderer, &clip) == 0;
}

void sb_graphics_clear(sb_graphics_t *graphics, uint8_t r, uint8_t g, uint8_t b) {
    if (!graphics) return;
    SDL_SetRenderDrawColor(graphics->renderer, r, g, b, 255);
    SDL_RenderClear(graphics->renderer);
}

void sb_graphics_rect(sb_graphics_t *graphics, int x, int y, int width,
                      int height, uint8_t r, uint8_t g, uint8_t b, bool filled) {
    if (!graphics) return;
    int64_t left = (int64_t)x - graphics->camera_x;
    int64_t top = (int64_t)y - graphics->camera_y;
    SDL_SetRenderDrawColor(graphics->renderer, r, g, b, 255);
    /* Preserve SDL's ordinary rectangle rasterization (including degenerate
     * dimensions) and single-call outline path. Wide fallback is needed only
     * when translated endpoints cannot safely be represented by SDL integers. */
    if (left >= INT_MIN && left <= INT_MAX && top >= INT_MIN && top <= INT_MAX &&
        left + width >= INT_MIN && left + width <= INT_MAX &&
        top + height >= INT_MIN && top + height <= INT_MAX) {
        SDL_Rect rect = {(int)left, (int)top, width, height};
        if (filled) SDL_RenderFillRect(graphics->renderer, &rect);
        else SDL_RenderDrawRect(graphics->renderer, &rect);
        return;
    }
    if (width <= 0 || height <= 0) return;
    if (filled) fill_screen_rect(graphics, left, top, width, height);
    else {
        fill_screen_rect(graphics, left, top, width, 1);
        fill_screen_rect(graphics, left, top + height - 1, width, 1);
        fill_screen_rect(graphics, left, top, 1, height);
        fill_screen_rect(graphics, left + width - 1, top, 1, height);
    }
}

bool sb_graphics_line(sb_graphics_t *graphics, int x1, int y1, int x2, int y2,
                       uint8_t r, uint8_t g, uint8_t b) {
    if (!graphics) return false;
    int64_t left = 0, top = 0, right = graphics->width, bottom = graphics->height;
    if (SDL_RenderIsClipEnabled(graphics->renderer)) {
        SDL_Rect clip;
        SDL_RenderGetClipRect(graphics->renderer, &clip);
        if (clip.x > left) left = clip.x;
        if (clip.y > top) top = clip.y;
        if ((int64_t)clip.x + clip.w < right) right = (int64_t)clip.x + clip.w;
        if ((int64_t)clip.y + clip.h < bottom) bottom = (int64_t)clip.y + clip.h;
    }
    if (left >= right || top >= bottom) return true;

    int64_t ax = (int64_t)x1 - graphics->camera_x;
    int64_t ay = (int64_t)y1 - graphics->camera_y;
    int64_t bx = (int64_t)x2 - graphics->camera_x;
    int64_t by = (int64_t)y2 - graphics->camera_y;
    if ((ax < left && bx < left) || (ax >= right && bx >= right) ||
        (ay < top && by < top) || (ay >= bottom && by >= bottom)) return true;

    int64_t dx = bx - ax, dy = by - ay;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    bool x_major = dx >= dy;
    int64_t start = x_major ? ax : ay, end = x_major ? bx : by;
    int64_t minor_start = x_major ? ay : ax, minor_end = x_major ? by : bx;
    /* Canonical direction makes endpoint reversal produce identical pixels. */
    if (start > end) {
        int64_t swap = start; start = end; end = swap;
        swap = minor_start; minor_start = minor_end; minor_end = swap;
    }
    int64_t first = x_major ? left : top;
    int64_t last = (x_major ? right : bottom) - 1;
    if (first < start) first = start;
    if (last > end) last = end;
    uint64_t major = (uint64_t)(end - start);
    int direction = minor_end >= minor_start ? 1 : -1;
    uint64_t minor = (uint64_t)(direction > 0 ? minor_end-minor_start : minor_start-minor_end);
    if (SDL_SetRenderDrawColor(graphics->renderer, r, g, b, 255) != 0) return false;

    SDL_Point points[128];
    int count = 0;
    for (int64_t position = first; position <= last; ++position) {
        uint64_t step = (uint64_t)(position - start);
        /* Evaluate the original line's raster phase, never rounded clipped
         * endpoints. On supported 32-bit-int targets step, minor, major are at
         * most UINT32_MAX, so step*minor + major/2 fits uint64_t. Translation
         * cancels from the deltas. Work is bounded by the visible major axis. */
        uint64_t offset = major ? (step * minor + major / 2) / major : 0;
        int64_t secondary = minor_start + direction * (int64_t)offset;
        int64_t x = x_major ? position : secondary;
        int64_t y = x_major ? secondary : position;
        if (x < left || x >= right || y < top || y >= bottom) continue;
        points[count++] = (SDL_Point){(int)x, (int)y};
        if (count == (int)(sizeof points / sizeof points[0])) {
            if (SDL_RenderDrawPoints(graphics->renderer, points, count) != 0) return false;
            count = 0;
        }
    }
    return count == 0 || SDL_RenderDrawPoints(graphics->renderer, points, count) == 0;
}

static void draw_char(sb_graphics_t *graphics, unsigned char c, int64_t x, int64_t y,
                      int scale) {
    if (c < 32 || c > 126) c = '?';
    const unsigned char *glyph = font8x8_basic[c];

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (glyph[row] & (1u << (7 - col))) {
                /* Clip in wide arithmetic before passing coordinates to SDL.
                 * Even a single scaled glyph can extend beyond INT_MAX. */
                int64_t left = x + (int64_t)col * scale;
                int64_t top = y + (int64_t)row * scale;
                int64_t right = left + scale, bottom = top + scale;
                if (left < 0) left = 0;
                if (top < 0) top = 0;
                if (right > graphics->width) right = graphics->width;
                if (bottom > graphics->height) bottom = graphics->height;
                if (left >= right || top >= bottom) continue;
                SDL_Rect pixel = {(int)left, (int)top,
                                  (int)(right - left), (int)(bottom - top)};
                SDL_RenderFillRect(graphics->renderer, &pixel);
            }
        }
    }
}

void sb_graphics_text(sb_graphics_t *graphics, const char *text, int x, int y,
                      uint8_t r, uint8_t g, uint8_t b, int scale) {
    if (!graphics || !text || scale <= 0) return;
    const int64_t origin_x = (int64_t)x - graphics->camera_x;
    int64_t cursor_x = origin_x, cursor_y = (int64_t)y - graphics->camera_y;
    SDL_SetRenderDrawColor(graphics->renderer, r, g, b, 255);
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        /* Rows only move down. Columns stop advancing once offscreen, but
         * keep scanning for a newline that can bring the next row back. This
         * bounds cursor arithmetic independently of string length. */
        if (cursor_y >= graphics->height) break;
        if (*p == '\n') {
            cursor_y += (int64_t)10 * scale;
            cursor_x = origin_x;
        } else if (cursor_x < graphics->width) {
            if (cursor_x + (int64_t)8 * scale > 0 &&
                cursor_y + (int64_t)8 * scale > 0)
                draw_char(graphics, *p, cursor_x, cursor_y, scale);
            cursor_x += (int64_t)8 * scale;
        }
    }
}

bool sb_graphics_begin_game_frame(sb_graphics_t *graphics) {
    if (!graphics || !graphics->game_frame) return false;

    if (SDL_SetRenderTarget(graphics->renderer, graphics->game_frame) != 0) {
        return false;
    }

    sb_graphics_camera_reset(graphics);
    sb_graphics_sprite_tint_reset(graphics);
    if (!sb_graphics_clip_reset(graphics) ||
        SDL_SetRenderDrawColor(graphics->renderer, 0, 0, 0, 255) != 0) {
        return false;
    }

    return SDL_RenderClear(graphics->renderer) == 0;
}

bool sb_graphics_show_game_frame(sb_graphics_t *graphics) {
    if (!graphics || !graphics->game_frame) return false;

    if (SDL_SetRenderTarget(graphics->renderer, NULL) != 0) {
        return false;
    }

    sb_graphics_camera_reset(graphics);
    sb_graphics_sprite_tint_reset(graphics);
    if (!sb_graphics_clip_reset(graphics) ||
        SDL_SetRenderDrawColor(graphics->renderer, 0, 0, 0, 255) != 0 ||
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
    int64_t left = (int64_t)x - graphics->camera_x;
    int64_t top = (int64_t)y - graphics->camera_y;
    /* Cull before narrowing. A visible positive-sized destination necessarily
     * has an origin representable as int, even for opposite camera extremes. */
    if (left >= graphics->width || top >= graphics->height ||
        left + dest_width <= 0 || top + dest_height <= 0) return true;
    SDL_Rect destination = { (int)left, (int)top, dest_width, dest_height };
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (flip_x) flip = (SDL_RendererFlip)(flip | SDL_FLIP_HORIZONTAL);
    if (flip_y) flip = (SDL_RendererFlip)(flip | SDL_FLIP_VERTICAL);
    /* Apply on every draw: textures may be shared by differently tinted sprites.
     * Pixel data is unchanged. A later draw overwrites both modulations even
     * if a previous setter or render call failed. */
    if (SDL_SetTextureColorMod(slot->texture, graphics->tint_r,
                              graphics->tint_g, graphics->tint_b) != 0 ||
        SDL_SetTextureAlphaMod(slot->texture, graphics->tint_a) != 0) return false;
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
    sb_graphics_camera_reset(graphics);
    sb_graphics_sprite_tint_reset(graphics);
    if (!sb_graphics_clip_reset(graphics)) SDL_Log("SugarBee: clip reset failed: %s", SDL_GetError());

    for (size_t i = 0; i < graphics->slot_count; ++i) {
        texture_slot_t *slot = &graphics->slots[i];
        if (!slot->texture) continue;

        sb_texture_t handle = ((uint32_t)slot->generation << 16) | (uint32_t)(i + 1);

        sb_graphics_texture_destroy(graphics, handle);
    }
}
