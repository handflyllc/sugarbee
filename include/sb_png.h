/* PNG decoding to packed RGBA8 pixels, independent of the renderer and Lua. */
#ifndef SB_PNG_H
#define SB_PNG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* v0.1 bound: at most 2048 x 2048 pixels, 16 MiB of decoded RGBA. */
#define SB_PNG_MAX_DIM 2048u

bool sb_png_decode_rgba(const uint8_t *encoded, size_t encoded_size,
                        uint8_t **pixels, int *width, int *height, size_t *length);
/* Caller frees the returned pixel buffer with free(). */
#endif
