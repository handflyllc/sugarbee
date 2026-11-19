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

/* Dimensions of the currently selected drawing canvas (game or host). */
int sb_graphics_width(const sb_graphics_t *graphics);
int sb_graphics_height(const sb_graphics_t *graphics);

/* Fixed cartridge canvas; replacement is allocated before releasing the old frame.
 * Selects the host on success. Internal callers may use any positive dimensions. */
bool sb_graphics_set_canvas(sb_graphics_t *graphics, int width, int height);
int sb_graphics_canvas_width(const sb_graphics_t *graphics);
int sb_graphics_canvas_height(const sb_graphics_t *graphics);
/* Select the host canvas without clearing the window. */
bool sb_graphics_begin_host_frame(sb_graphics_t *graphics);

/* Multiplicative sprite-only RGB/alpha modulation, 0..255 per channel.
 * Replaces state; reset selects white/full opacity. Pixel data is unchanged.
 * Frame start, host presentation and cartridge cleanup restore defaults. */
void sb_graphics_sprite_tint(sb_graphics_t *graphics, uint8_t r, uint8_t g,
                             uint8_t b, uint8_t a);
void sb_graphics_sprite_tint_reset(sb_graphics_t *graphics);

/* Drawing uses screen = world - camera for rectangles, lines, circles, triangles,
 * text and sprites.
 * Replaces the position; reset is (0,0). Clip and clear remain screen-space.
 * Frame start, host presentation and cartridge cleanup reset the camera. */
void sb_graphics_camera(sb_graphics_t *graphics, int x, int y);
void sb_graphics_camera_reset(sb_graphics_t *graphics);

/* Replace the clip with a screen-logical rectangle intersected with the viewport.
 * Zero dimensions hide drawing; negative dimensions fail without changing state.
 * Edges are half-open. Clear ignores the clip. No coordinate translation. */
bool sb_graphics_clip(sb_graphics_t *graphics, int x, int y, int width, int height);
bool sb_graphics_clip_reset(sb_graphics_t *graphics);

/* Colors are RGB, with each channel in the range 0..255 and opaque alpha. */
void sb_graphics_clear(sb_graphics_t *graphics, uint8_t r, uint8_t g, uint8_t b);
void sb_graphics_rect(sb_graphics_t *graphics, int x, int y, int width,
                      int height, uint8_t r, uint8_t g, uint8_t b, bool filled);

/* Opaque one-logical-pixel line, both endpoints included; equal endpoints draw
 * one pixel. Camera and clip apply, sprite tint does not. Canonical major-axis
 * integer rasterization is endpoint-reversal invariant and preserves phase
 * through clipping. Work is bounded by viewport dimensions, not line length.
 * Returns false for a null context or SDL drawing failure. */
bool sb_graphics_line(sb_graphics_t *graphics, int x1, int y1, int x2, int y2,
                       uint8_t r, uint8_t g, uint8_t b);

/* Integer disk: dx*dx + dy*dy <= radius*radius. Outline is its four-neighbor
 * boundary. Radius zero draws one pixel; negative radius/null context fails.
 * Opaque RGB, camera/clip aware, independent of tint. Work bounded by visible
 * rows, including extreme coordinates/radii. False on renderer failure. */
bool sb_graphics_circle(sb_graphics_t *graphics, int cx, int cy, int radius,
                        uint8_t r, uint8_t g, uint8_t b);
bool sb_graphics_circle_fill(sb_graphics_t *graphics, int cx, int cy, int radius,
                             uint8_t r, uint8_t g, uint8_t b);

/* Opaque triangle at pixel centers with top-left edge ownership. Vertex order
 * is irrelevant; zero area draws nothing. Camera/clip apply, tint does not.
 * Work bounded by visible rows; false for null context or renderer failure. */
bool sb_graphics_triangle_fill(sb_graphics_t *graphics,
                               int x1, int y1, int x2, int y2, int x3, int y3,
                               uint8_t r, uint8_t g, uint8_t b);

/* Current byte-oriented bitmap font layout; drawing and measurement share these. */
#define SB_FONT_CELL_WIDTH 8
#define SB_FONT_CELL_HEIGHT 8
#define SB_FONT_LINE_ADVANCE 10
#define SB_TEXT_MEASURE_MAX_BYTES (256u * 1024u)
/* Measure character-cell bounds without a graphics context or drawing state.
 * Newline advances a line; NUL ends text. Empty/nonpositive scale gives (0,0).
 * Length includes any bytes after NUL and must not exceed the cap. False for
 * invalid pointers, oversize input, or dimensions exceeding INT_MAX; outputs
 * are zeroed when both output pointers are valid. No allocation or rendering. */
bool sb_graphics_measure_text(const char *text, size_t length, int scale,
                              int *width, int *height);

/* Temporary 8x8 bitmap font; scale is logical pixels per bitmap pixel. */
/* Text arithmetic is clipped safely to the viewport for all int coordinates
 * and scales. Nonpositive scale draws nothing. */
void sb_graphics_text(sb_graphics_t *graphics, const char *text, int x, int y,
                      uint8_t r, uint8_t g, uint8_t b, int scale);

/* Per-call opacity, ordinary source-over blend. Zero draws nothing; 255 is
 * opaque. Camera/clip apply; sprite tint does not. Legacy entry points remain. */
void sb_graphics_rect_alpha(sb_graphics_t *graphics, int x, int y, int width,
                           int height, uint8_t r, uint8_t g, uint8_t b,
                           bool filled, uint8_t alpha);
void sb_graphics_text_alpha(sb_graphics_t *graphics, const char *text, int x, int y,
                           uint8_t r, uint8_t g, uint8_t b, int scale, uint8_t alpha);

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

/* Release all cartridge texture handles; preserve host graphics resources. */
void sb_graphics_clear_cartridge_textures(sb_graphics_t *graphics);

/* Select and clear the retained game frame for fresh cartridge drawing. */
bool sb_graphics_begin_game_frame(sb_graphics_t *graphics);

/* Select the window and copy the retained frame; host overlays follow. */
bool sb_graphics_show_game_frame(sb_graphics_t *graphics);

/* Called once after drawing the current frame. */
void sb_graphics_present(sb_graphics_t *graphics);

#endif /* SB_GRAPHICS_H */
