/* Bitmap-font cell metrics, independent of rendering, camera and clipping. */
#include "sb_graphics.h"
#include <limits.h>

bool sb_graphics_measure_text(const char *text, size_t length, int scale,
                              int *width, int *height) {
    if (!width || !height) return false;
    *width = 0; *height = 0;
    if (!text || length > SB_TEXT_MEASURE_MAX_BYTES) return false;
    if (length == 0 || text[0] == '\0' || scale <= 0) return true;

    /* A nonempty string has at least one line, including whitespace-only lines.
     * Wide arithmetic handles INT_MAX scale before any narrowing. */
    int64_t line_width = 0, max_width = 0;
    int64_t total_height = (int64_t)SB_FONT_CELL_HEIGHT * scale;
    if (total_height > INT_MAX) return false;
    for (size_t i = 0; i < length && text[i] != '\0'; ++i) {
        if (text[i] == '\n') {
            total_height += (int64_t)SB_FONT_LINE_ADVANCE * scale;
            if (total_height > INT_MAX) return false;
            line_width = 0;
        } else {
            line_width += (int64_t)SB_FONT_CELL_WIDTH * scale;
            if (line_width > INT_MAX) return false;
            if (line_width > max_width) max_width = line_width;
        }
    }
    *width = (int)max_width;
    *height = (int)total_height;
    return true;
}
