/* SDL2 backend for SugarBee's language-neutral 2D graphics interface. */
#include "sb_graphics.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
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
    int host_width, host_height;
    int canvas_width, canvas_height;
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
    graphics->width = graphics->host_width = graphics->canvas_width = width;
    graphics->height = graphics->host_height = graphics->canvas_height = height;

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
        !sb_graphics_begin_host_frame(graphics)) {
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

/* Disjoint border strips prevent corners blending twice, including 1px shapes. */
void sb_graphics_rect_alpha(sb_graphics_t *graphics, int x, int y, int width,
                           int height, uint8_t r, uint8_t g, uint8_t b,
                           bool filled, uint8_t alpha) {
    if (!graphics || alpha == 0) return;
    if (alpha == 255) {
        sb_graphics_rect(graphics, x, y, width, height, r, g, b, filled);
        return;
    }
    if (width <= 0 || height <= 0) return;
    SDL_BlendMode old;
    if (SDL_GetRenderDrawBlendMode(graphics->renderer, &old) != 0) return;
    if (SDL_SetRenderDrawBlendMode(graphics->renderer, SDL_BLENDMODE_BLEND) != 0) return;
    SDL_SetRenderDrawColor(graphics->renderer, r, g, b, alpha);
    int64_t left = (int64_t)x - graphics->camera_x;
    int64_t top = (int64_t)y - graphics->camera_y;
    if (filled) fill_screen_rect(graphics, left, top, width, height);
    else {
        fill_screen_rect(graphics, left, top, width, 1);
        if (height > 1) fill_screen_rect(graphics, left, top + height - 1, width, 1);
        if (height > 2) {
            fill_screen_rect(graphics, left, top + 1, 1, height - 2);
            if (width > 1) fill_screen_rect(graphics, left + width - 1, top + 1, 1, height - 2);
        }
    }
    SDL_SetRenderDrawBlendMode(graphics->renderer, old);
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

/* Exact floor(sqrt(value)), with a fixed maximum of 32 iterations. */
static uint64_t circle_isqrt(uint64_t value) {
    uint64_t root = 0, bit = UINT64_C(1) << 62;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

/* Reject before squaring: camera translation can exceed the radius range. */
static int64_t circle_extent(int radius, int64_t dy) {
    if (dy < -(int64_t)radius || dy > radius) return -1;
    return (int64_t)circle_isqrt((uint64_t)radius * (uint64_t)radius -
                                (uint64_t)(dy * dy));
}

static bool circle_span(sb_graphics_t *graphics, int64_t x1, int64_t x2,
                        int y, int64_t left, int64_t right) {
    if (x1 < left) x1 = left;
    if (x2 >= right) x2 = right - 1;
    if (x1 > x2) return true;
    SDL_Rect span = {(int)x1, y, (int)(x2 - x1 + 1), 1};
    return SDL_RenderFillRect(graphics->renderer, &span) == 0;
}

static bool draw_circle(sb_graphics_t *graphics, int cx, int cy, int radius,
                        uint8_t r, uint8_t g, uint8_t b, bool filled) {
    if (!graphics || radius < 0) return false;
    int64_t left = 0, top = 0, right = graphics->width, bottom = graphics->height;
    if (SDL_RenderIsClipEnabled(graphics->renderer)) {
        SDL_Rect clip;
        SDL_RenderGetClipRect(graphics->renderer, &clip);
        if (clip.x > left) left = clip.x;
        if (clip.y > top) top = clip.y;
        if ((int64_t)clip.x + clip.w < right) right = (int64_t)clip.x + clip.w;
        if ((int64_t)clip.y + clip.h < bottom) bottom = (int64_t)clip.y + clip.h;
    }
    int64_t x = (int64_t)cx - graphics->camera_x;
    int64_t y = (int64_t)cy - graphics->camera_y;
    if (left >= right || top >= bottom || x + radius < left || x - radius >= right)
        return true;
    if (top < y - radius) top = y - radius;
    if (bottom > y + radius + 1) bottom = y + radius + 1;
    if (top >= bottom) return true;
    if (SDL_SetRenderDrawColor(graphics->renderer, r, g, b, 255) != 0) return false;
    /* Only visible rows are visited, even at INT_MAX radius. Shape is computed
     * before clipping, so clipping cannot introduce a false boundary. */
    for (int64_t row = top; row < bottom; ++row) {
        int64_t dy = row - y, extent = circle_extent(radius, dy);
        if (filled) {
            if (!circle_span(graphics, x-extent, x+extent, (int)row, left, right))
                return false;
        } else {
            /* Interior pixels have all four axial neighbors inside the disk. */
            int64_t interior = extent - 1;
            int64_t above = circle_extent(radius, dy-1);
            int64_t below = circle_extent(radius, dy+1);
            if (above < interior) interior = above;
            if (below < interior) interior = below;
            if (interior < 0) {
                if (!circle_span(graphics, x-extent, x+extent, (int)row, left, right))
                    return false;
            } else {
                if (!circle_span(graphics, x-extent, x-interior-1, (int)row, left, right) ||
                    !circle_span(graphics, x+interior+1, x+extent, (int)row, left, right))
                    return false;
            }
        }
    }
    return true;
}

bool sb_graphics_circle(sb_graphics_t *graphics, int cx, int cy, int radius,
                       uint8_t r, uint8_t g, uint8_t b) {
    return draw_circle(graphics, cx, cy, radius, r, g, b, false);
}

bool sb_graphics_circle_fill(sb_graphics_t *graphics, int cx, int cy, int radius,
                            uint8_t r, uint8_t g, uint8_t b) {
    return draw_circle(graphics, cx, cy, radius, r, g, b, true);
}

/* First pixel center on or to the right of an edge at row + 0.5:
 * ceil(edge_x - 0.5). Endpoints are sorted by y and this row crosses the edge.
 * Split dx*t into quotient/remainder before applying the half-pixel terms:
 * full signed32 endpoint differences fit uint32, their product fits uint64,
 * whereas doubling that product (or a full-range cross product) may overflow. */
static int64_t triangle_edge_start(int64_t ax, int64_t ay, int64_t bx,
                                   int64_t by, int64_t row) {
    int64_t dx = bx - ax;
    uint64_t width = (uint64_t)(dx < 0 ? -dx : dx);
    uint64_t height = (uint64_t)(by - ay);
    uint64_t product = width * (uint64_t)(row - ay);
    int64_t whole = (int64_t)(product / height);
    int64_t remainder = (int64_t)(product % height);
    int64_t fraction = 2 * remainder + (int64_t)width;
    if (dx < 0) { whole = -whole; fraction = -fraction; }
    int64_t numerator = fraction - (int64_t)height;
    int64_t denominator = 2 * (int64_t)height;
    /* C truncates toward zero; add one only for a positive remainder. */
    int64_t ceiling = numerator / denominator + (numerator % denominator > 0);
    return ax + whole + ceiling;
}

bool sb_graphics_triangle_fill(sb_graphics_t *graphics,
                               int x1, int y1, int x2, int y2, int x3, int y3,
                               uint8_t r, uint8_t g, uint8_t b) {
    if (!graphics) return false;
    int64_t x[3] = {(int64_t)x1 - graphics->camera_x,
                    (int64_t)x2 - graphics->camera_x,
                    (int64_t)x3 - graphics->camera_x};
    int64_t y[3] = {(int64_t)y1 - graphics->camera_y,
                    (int64_t)y2 - graphics->camera_y,
                    (int64_t)y3 - graphics->camera_y};
    int64_t left = 0, top = 0, right = graphics->width, bottom = graphics->height;
    if (SDL_RenderIsClipEnabled(graphics->renderer)) {
        SDL_Rect clip;
        SDL_RenderGetClipRect(graphics->renderer, &clip);
        if (clip.x > left) left = clip.x;
        if (clip.y > top) top = clip.y;
        if ((int64_t)clip.x + clip.w < right) right = (int64_t)clip.x + clip.w;
        if ((int64_t)clip.y + clip.h < bottom) bottom = (int64_t)clip.y + clip.h;
    }
    int64_t min_y = y[0], max_y = y[0], min_x = x[0], max_x = x[0];
    for (int i = 1; i < 3; ++i) {
        if (y[i] < min_y) min_y = y[i];
        if (y[i] > max_y) max_y = y[i];
        if (x[i] < min_x) min_x = x[i];
        if (x[i] > max_x) max_x = x[i];
    }
    if (top < min_y) top = min_y;
    if (bottom > max_y) bottom = max_y;
    if (left >= right || top >= bottom || max_x <= left || min_x >= right)
        return true;
    if (SDL_SetRenderDrawColor(graphics->renderer, r, g, b, 255) != 0) return false;
    for (int64_t row = top; row < bottom; ++row) {
        int64_t ends[2];
        int count = 0;
        for (int i = 0; i < 3; ++i) {
            int a = i, z = (i + 1) % 3;
            if (y[a] > y[z]) { int swap = a; a = z; z = swap; }
            /* Horizontal edges never cross a pixel-center row. Exactly two
             * edges cross any other occupied row, even with repeated vertices. */
            if (row < y[a] || row >= y[z]) continue;
            ends[count++] = triangle_edge_start(x[a], y[a], x[z], y[z], row);
        }
        if (count != 2) continue;
        int64_t start = ends[0] < ends[1] ? ends[0] : ends[1];
        int64_t end = ends[0] > ends[1] ? ends[0] : ends[1];
        /* Left inclusive, right exclusive: a shared edge has exactly one
         * owner. Collinear triangles have equal ends and draw nothing. */
        if (start < left) start = left;
        if (end > right) end = right;
        if (start >= end) continue;
        SDL_Rect span = {(int)start, (int)row, (int)(end - start), 1};
        if (SDL_RenderFillRect(graphics->renderer, &span) != 0) return false;
    }
    return true;
}

static void draw_char(sb_graphics_t *graphics, unsigned char c, int64_t x, int64_t y,
                      int scale) {
    if (c < 32 || c > 126) c = '?';
    const unsigned char *glyph = font8x8_basic[c];

    for (int row = 0; row < SB_FONT_CELL_HEIGHT; row++) {
        for (int col = 0; col < SB_FONT_CELL_WIDTH; col++) {
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

void sb_graphics_text_alpha(sb_graphics_t *graphics, const char *text, int x, int y,
                      uint8_t r, uint8_t g, uint8_t b, int scale, uint8_t alpha) {
    if (!graphics || !text || scale <= 0 || alpha == 0) return;
    SDL_BlendMode old;
    if (SDL_GetRenderDrawBlendMode(graphics->renderer, &old) != 0) return;
    if (SDL_SetRenderDrawBlendMode(graphics->renderer,
            alpha == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND) != 0) return;
    const int64_t origin_x = (int64_t)x - graphics->camera_x;
    int64_t cursor_x = origin_x, cursor_y = (int64_t)y - graphics->camera_y;
    SDL_SetRenderDrawColor(graphics->renderer, r, g, b, alpha);
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        /* Rows only move down. Columns stop advancing once offscreen, but
         * keep scanning for a newline that can bring the next row back. This
         * bounds cursor arithmetic independently of string length. */
        if (cursor_y >= graphics->height) break;
        if (*p == '\n') {
            cursor_y += (int64_t)SB_FONT_LINE_ADVANCE * scale;
            cursor_x = origin_x;
        } else if (cursor_x < graphics->width) {
            if (cursor_x + (int64_t)SB_FONT_CELL_WIDTH * scale > 0 &&
                cursor_y + (int64_t)SB_FONT_CELL_HEIGHT * scale > 0)
                draw_char(graphics, *p, cursor_x, cursor_y, scale);
            cursor_x += (int64_t)SB_FONT_CELL_WIDTH * scale;
        }
    }
    SDL_SetRenderDrawBlendMode(graphics->renderer, old);
}
void sb_graphics_text(sb_graphics_t *graphics, const char *text, int x, int y,
                      uint8_t r, uint8_t g, uint8_t b, int scale) {
    sb_graphics_text_alpha(graphics, text, x, y, r, g, b, scale, 255);
}

int sb_graphics_canvas_width(const sb_graphics_t *graphics) {
    return graphics ? graphics->canvas_width : 0;
}
int sb_graphics_canvas_height(const sb_graphics_t *graphics) {
    return graphics ? graphics->canvas_height : 0;
}

bool sb_graphics_begin_host_frame(sb_graphics_t *graphics) {
    if (!graphics || SDL_SetRenderTarget(graphics->renderer, NULL) != 0 ||
        SDL_RenderSetLogicalSize(graphics->renderer, graphics->host_width,
                                 graphics->host_height) != 0) return false;
    graphics->width = graphics->host_width;
    graphics->height = graphics->host_height;
    sb_graphics_camera_reset(graphics);
    sb_graphics_sprite_tint_reset(graphics);
    return sb_graphics_clip_reset(graphics);
}

bool sb_graphics_set_canvas(sb_graphics_t *graphics, int width, int height) {
    if (!graphics || width <= 0 || height <= 0) return false;
    SDL_Texture *frame = SDL_CreateTexture(graphics->renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, width, height);
    if (!frame) return false;
    if (SDL_SetTextureBlendMode(frame, SDL_BLENDMODE_NONE) != 0 ||
        SDL_SetTextureScaleMode(frame, SDL_ScaleModeNearest) != 0 ||
        !sb_graphics_begin_host_frame(graphics)) {
        SDL_DestroyTexture(frame);
        return false;
    }
    SDL_Texture *old = graphics->game_frame;
    int old_w = graphics->canvas_width, old_h = graphics->canvas_height;
    graphics->game_frame = frame;
    graphics->canvas_width = width; graphics->canvas_height = height;
    bool ok = sb_graphics_begin_game_frame(graphics);
    bool host_ok = sb_graphics_begin_host_frame(graphics);
    if (!ok || !host_ok) {
        graphics->game_frame = old;
        graphics->canvas_width = old_w; graphics->canvas_height = old_h;
        SDL_DestroyTexture(frame);
        return false;
    }
    SDL_DestroyTexture(old);
    return true;
}

bool sb_graphics_begin_game_frame(sb_graphics_t *graphics) {
    if (!graphics || !graphics->game_frame ||
        SDL_SetRenderTarget(graphics->renderer, graphics->game_frame) != 0 ||
        SDL_RenderSetLogicalSize(graphics->renderer, 0, 0) != 0 ||
        SDL_RenderSetViewport(graphics->renderer, NULL) != 0 ||
        SDL_RenderSetScale(graphics->renderer, 1, 1) != 0) return false;
    graphics->width = graphics->canvas_width;
    graphics->height = graphics->canvas_height;
    sb_graphics_camera_reset(graphics);
    sb_graphics_sprite_tint_reset(graphics);
    if (!sb_graphics_clip_reset(graphics) ||
        SDL_SetRenderDrawColor(graphics->renderer, 0, 0, 0, 255) != 0) return false;
    return SDL_RenderClear(graphics->renderer) == 0;
}

bool sb_graphics_show_game_frame(sb_graphics_t *graphics) {
    if (!graphics || !graphics->game_frame ||
        SDL_SetRenderTarget(graphics->renderer, NULL) != 0 ||
        SDL_RenderSetLogicalSize(graphics->renderer, 0, 0) != 0 ||
        SDL_RenderSetViewport(graphics->renderer, NULL) != 0 ||
        SDL_RenderSetScale(graphics->renderer, 1, 1) != 0 ||
        !sb_graphics_clip_reset(graphics)) return false;
    int output_w, output_h;
    if (SDL_GetRendererOutputSize(graphics->renderer, &output_w, &output_h) != 0 ||
        output_w <= 0 || output_h <= 0) return false;
    int w = graphics->canvas_width, h = graphics->canvas_height;
    int scale = output_w / w;
    if (output_h / h < scale) scale = output_h / h;
    SDL_Rect dest = {0, 0, w, h};
    if (scale >= 1) { dest.w = w * scale; dest.h = h * scale; }
    else if ((int64_t)output_w * h <= (int64_t)output_h * w) {
        dest.w = output_w; dest.h = (int)((int64_t)h * output_w / w);
    } else {
        dest.h = output_h; dest.w = (int)((int64_t)w * output_h / h);
    }
    if (dest.w < 1) dest.w = 1;
    if (dest.h < 1) dest.h = 1;
    dest.x = (output_w - dest.w) / 2; dest.y = (output_h - dest.h) / 2;
    bool ok = SDL_SetRenderDrawColor(graphics->renderer, 0, 0, 0, 255) == 0 &&
              SDL_RenderClear(graphics->renderer) == 0 &&
              SDL_RenderCopy(graphics->renderer, graphics->game_frame, NULL, &dest) == 0;
    /* Overlays use host coordinates; retained game pixels stay untouched. */
    bool host_ok = sb_graphics_begin_host_frame(graphics);
    return ok && host_ok;
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

/* SDL's software rotation uses temporary surfaces. Bound the destination size
 * rather than allowing a huge destination to allocate before clipping. */
bool sb_graphics_sprite_region_rotated(sb_graphics_t *graphics, sb_texture_t texture,
        int x, int y, int sx, int sy, int sw, int sh, int dw, int dh,
        double angle, bool custom_pivot, double px, double py) {
    texture_slot_t *slot = find_texture(graphics, texture);
    if (!slot || sx < 0 || sy < 0 || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 ||
        sw > slot->width || sh > slot->height || sx > slot->width-sw ||
        sy > slot->height-sh || !isfinite(angle)) return false;
    /* Validate before culling: offscreen and zero-angle calls share the limits. */
    if (dw > SB_ROTATED_MAX_SIDE || dh > SB_ROTATED_MAX_SIDE ||
        (uint64_t)dw*(uint64_t)dh > SB_ROTATED_MAX_PIXELS) return false;
    if (!custom_pivot) { px=dw/2.0; py=dh/2.0; }
    if (!isfinite(px) || !isfinite(py) || px < INT_MIN || px > INT_MAX ||
        py < INT_MIN || py > INT_MAX) return false;
    angle=fmod(angle,360.0);
    if (angle<0) angle+=360.0;
    if (angle==0) return sb_graphics_sprite_region(graphics,texture,x,y,
        sx,sy,sw,sh,dw,dh,false,false);
    double c,sn;
    if (angle==90) { c=0; sn=1; }
    else if (angle==180) { c=-1; sn=0; }
    else if (angle==270) { c=0; sn=-1; }
    else { double rad=angle*(3.14159265358979323846/180.0); c=cos(rad); sn=sin(rad); }
    /* Express any custom pivot as a shifted center rotation, keeping large
     * pivot/camera coordinates out of SDL's float/int intermediate arithmetic. */
    double rx=dw/2.0-px, ry=dh/2.0-py;
    double cx=(double)x-graphics->camera_x+px+c*rx-sn*ry;
    double cy=(double)y-graphics->camera_y+py+sn*rx+c*ry;
    double hw=(fabs(c)*dw+fabs(sn)*dh)/2.0;
    double hh=(fabs(sn)*dw+fabs(c)*dh)/2.0;
    double l=0,t=0,r=graphics->width,bt=graphics->height;
    if (SDL_RenderIsClipEnabled(graphics->renderer)) {
        SDL_Rect clip; SDL_RenderGetClipRect(graphics->renderer,&clip);
        l=fmax(l,clip.x); t=fmax(t,clip.y);
        r=fmin(r,(double)clip.x+clip.w); bt=fmin(bt,(double)clip.y+clip.h);
    }
    if (l>=r || t>=bt || cx+hw<l || cy+hh<t || cx-hw>r || cy-hh>bt) return true;
    SDL_Rect source={sx,sy,sw,sh};
    SDL_FRect destination={(float)(cx-dw/2.0),(float)(cy-dh/2.0),(float)dw,(float)dh};
    SDL_FPoint center={(float)(dw/2.0),(float)(dh/2.0)};
    if (SDL_SetTextureColorMod(slot->texture,graphics->tint_r,graphics->tint_g,
                              graphics->tint_b)!=0 ||
        SDL_SetTextureAlphaMod(slot->texture,graphics->tint_a)!=0) return false;
    return SDL_RenderCopyExF(graphics->renderer,slot->texture,&source,&destination,
                             angle,&center,SDL_FLIP_NONE)==0;
}

bool sb_graphics_sprite_rotated(sb_graphics_t *graphics, sb_texture_t texture,
        int x, int y, double angle, bool custom_pivot, double px, double py) {
    texture_slot_t *slot=find_texture(graphics,texture);
    return slot && sb_graphics_sprite_region_rotated(graphics,texture,x,y,
        0,0,slot->width,slot->height,slot->width,slot->height,angle,custom_pivot,px,py);
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
