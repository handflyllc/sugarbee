#include "sb_png.h"
#include <png.h>
#include <stdlib.h>

bool sb_png_decode_rgba(const uint8_t *encoded, size_t encoded_size,
                        uint8_t **pixels, int *width, int *height, size_t *length) {
    if (pixels) *pixels = NULL;
    if (width) *width = 0;
    if (height) *height = 0;
    if (length) *length = 0;
    if (!encoded || !encoded_size || !pixels || !width || !height || !length)
        return false;

    png_image image = {0};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image, encoded, encoded_size))
        return false;

    if (!image.width || !image.height ||
        image.width > SB_PNG_MAX_DIM || image.height > SB_PNG_MAX_DIM) {
        png_image_free(&image);
        return false;
    }
    image.format = PNG_FORMAT_RGBA;
    size_t bytes = (size_t)image.width * (size_t)image.height * 4u;
    uint8_t *buf = malloc(bytes);
    if (!buf) { png_image_free(&image); return false; }
    if (!png_image_finish_read(&image, NULL, buf, 0, NULL)) {
        free(buf);
        png_image_free(&image);
        return false;
    }
    *width = (int)image.width;
    *height = (int)image.height;
    *length = bytes;
    *pixels = buf;
    png_image_free(&image);
    return true;
}
