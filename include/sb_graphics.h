/*
 * SugarBee's internal, language-neutral 2D graphics services (unstable v0.1).
 * No SDL or Lua types are exposed by this interface.
 */
#ifndef SB_GRAPHICS_H
#define SB_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>

#define SB_DEFAULT_WIDTH 1280
#define SB_DEFAULT_HEIGHT 720

typedef struct sb_graphics sb_graphics_t;

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

/* Called once after drawing the current frame. */
void sb_graphics_present(sb_graphics_t *graphics);

#endif /* SB_GRAPHICS_H */
