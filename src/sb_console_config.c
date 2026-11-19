#include "sb_console_config.h"
#include "sb_graphics.h"
#include "sb_assets.h"
#include "yyjson.h"
#include <stdio.h>
#include <stdlib.h>

static bool fail(char *error, size_t cap, const char *message) {
    if (cap) snprintf(error, cap, "console.json: %s", message);
    return false;
}
bool sb_console_config_parse(const void *data, size_t size, int *width, int *height,
                             char *error, size_t cap) {
    *width = SB_DEFAULT_WIDTH; *height = SB_DEFAULT_HEIGHT;
    if (cap) error[0] = '\0';
    if (!data || !size || size > SB_CONSOLE_CONFIG_MAX)
        return fail(error, cap, "expected JSON object, at most 4096 bytes");
    yyjson_doc *doc = yyjson_read((const char *)data, size, 0);
    if (!doc) return fail(error, cap, "invalid JSON");
    const char *problem = NULL;
    yyjson_val *root = yyjson_doc_get_root(doc), *video = NULL;
    size_t i, count; yyjson_val *key, *value;
    if (!yyjson_is_obj(root)) problem = "root must be an object";
    else yyjson_obj_foreach(root, i, count, key, value) {
        if (!yyjson_equals_str(key, "video")) { problem = "unknown root field"; break; }
        if (video) { problem = "duplicate video field"; break; }
        video = value;
    }
    int w = SB_DEFAULT_WIDTH, h = SB_DEFAULT_HEIGHT;
    if (!problem && video) {
        yyjson_val *wv = NULL, *hv = NULL;
        if (!yyjson_is_obj(video)) problem = "video must be an object";
        else yyjson_obj_foreach(video, i, count, key, value) {
            yyjson_val **slot = NULL;
            if (yyjson_equals_str(key, "width")) slot = &wv;
            else if (yyjson_equals_str(key, "height")) slot = &hv;
            else { problem = "unknown video field"; break; }
            if (*slot) { problem = "duplicate video dimension"; break; }
            *slot = value;
        }
        if (!problem) {
            if (!yyjson_is_uint(wv) || !yyjson_is_uint(hv) ||
                yyjson_get_uint(wv) > 1280 || yyjson_get_uint(hv) > 720)
                problem = "width and height must be supported positive JSON integers";
            else {
                w = (int)yyjson_get_uint(wv); h = (int)yyjson_get_uint(hv);
                if (!((w == 300 && h == 200) || (w == 320 && h == 240) ||
                      (w == 640 && h == 360) || (w == 960 && h == 720) ||
                      (w == 1280 && h == 720))) problem = "unsupported resolution";
            }
        }
    }
    yyjson_doc_free(doc);
    if (problem) return fail(error, cap, problem);
    *width = w; *height = h;
    return true;
}
bool sb_console_config_load(const char *cart, int *width, int *height,
                            char *error, size_t cap) {
    *width = SB_DEFAULT_WIDTH; *height = SB_DEFAULT_HEIGHT;
    if (cap) error[0] = '\0';
    uint8_t *data = NULL; size_t size = 0;
    sb_asset_result_t result = sb_asset_read_limited(cart, "console.json",
                                                    SB_CONSOLE_CONFIG_MAX, &data, &size);
    if (result == SB_ASSET_NOT_FOUND) return true;
    if (result != SB_ASSET_OK) return fail(error, cap, sb_asset_error_string(result));
    bool ok = sb_console_config_parse(data, size, width, height, error, cap);
    free(data);
    return ok;
}
