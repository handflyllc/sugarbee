/*
 * SugarBee's internal, language-neutral 2D graphics services (unstable v0.1).
 * No SDL or Lua types are exposed by this interface.
 */
#ifndef SB_GRAPHICS_H
#define SB_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define SB_DEFAULT_WIDTH 1280
#define SB_DEFAULT_HEIGHT 720

typedef struct sb_graphics sb_graphics_t;
/* 0 is never a valid texture handle. Handles belong to one graphics context. */
typedef uint32_t sb_texture_t;
#define SB_TEXTURE_INVALID ((sb_texture_t)0)


/*
 * Requires SDL's video subsystem to have been initialized by the host.
 * Owns the SDL window and renderer. Returns NULL on initialization failure.
 * All drawing and lifecycle calls must occur on the host's video thread.
 */
sb_graphics_t *sb_graphics_create(const char *title, int width, int height);
void sb_graphics_destroy(sb_graphics_t *graphics);

/* Logical (game) dimensions, which may differ from the display dimensions. */
int sb_graphics_width(const sb_graphics_t *graphics);
int sb_graphics_height(const sb_graphics_t *graphics);

/* Colors are RGB, with each channel in the range 0..255 and opaque alpha. */
void sb_graphics_clear(sb_graphics_t *graphics, uint8_t r, uint8_t g, uint8_t b);
void sb_graphics_rect(sb_graphics_t *graphics, int x, int y, int width,
                      int height, uint8_t r, uint8_t g, uint8_t b, bool filled);

/* Temporary 8x8 bitmap font; scale is logical pixels per bitmap pixel. */
void sb_graphics_text(sb_graphics_t *graphics, const char *text, int x, int y,
                      uint8_t r, uint8_t g, uint8_t b, int scale);

/*
 * Upload packed RGBA8 pixels, row-major, four bytes per pixel (R,G,B,A).
 * Length must equal width * height * 4.  Pixels are copied; caller keeps
 * ownership of its buffer. Returns 0 on failure. No filesystem access here.
 * The graphics context owns the texture until texture_destroy or shutdown.
 */
sb_texture_t sb_graphics_texture_rgba(sb_graphics_t *graphics,
                                      int width, int height,
                                      const uint8_t *rgba, size_t length);

/* Draw the texture at native dimensions in logical pixel coordinates.
 * Returns false for unknown/stale handles or SDL rendering failure.
 */
bool sb_graphics_sprite(sb_graphics_t *graphics, sb_texture_t texture,
                        int x, int y);

/* Draw a source rectangle from a texture into a destination rectangle.
 * All rectangles use integer logical pixels. The source must be entirely
 * inside the texture; widths and heights must be positive. Destination
 * dimensions may differ from source dimensions (nearest-neighbor scaling).
 * Flip flags mirror within the destination, without changing its position.
 * Returns false on invalid dimensions, stale handles, or SDL failure.
 */
bool sb_graphics_sprite_region(sb_graphics_t *graphics, sb_texture_t texture,
                               int x, int y,
                               int src_x, int src_y, int src_width, int src_height,
                               int dest_width, int dest_height,
                               bool flip_x, bool flip_y);

/* Release a texture. Returns false for unknown/stale handles. */
bool sb_graphics_texture_destroy(sb_graphics_t *graphics, sb_texture_t texture);

/* Called once after drawing the current frame. */
void sb_graphics_present(sb_graphics_t *graphics);

#endif /* SB_GRAPHICS_H */
