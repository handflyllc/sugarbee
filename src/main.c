/*
 * PROJECT SUGARBEE - C Host Engine
 * Target Platform: Raspberry Pi 4 (DRM/KMS / Linux Desktop)
 * Language: C99 / SDL2 / Embedded Lua 5.4 / zlib & minizip
 */

#define _POSIX_C_SOURCE 200809L
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

/* Cartridge asset readers and PNG decoding are outside the Lua adapter. */
#include "sb_assets.h"
#include "sb_png.h"

#include "vcart.h"
#include "sb_graphics.h"
#include "sb_input.h"
#include "sb_timing.h"

#define SB_LAUNCHER_MAX_CARTRIDGES 64
#define SB_LAUNCHER_PATH_SIZE 4096

/* Provisional per-call Lua instruction allowances, not wall-clock limits. */
#define SB_LUA_STARTUP_INSTRUCTION_LIMIT 10000000
#define SB_LUA_UPDATE_INSTRUCTION_LIMIT 1000000
#define SB_LUA_DRAW_INSTRUCTION_LIMIT 1000000
#define SB_LUA_SHUTDOWN_INSTRUCTION_LIMIT 1000000

typedef struct {
    char title[32];
    char path[SB_LAUNCHER_PATH_SIZE];
} launcher_cartridge_t;

typedef enum {
    PAUSE_MENU_RESUME,
    PAUSE_MENU_EXIT
} pause_menu_selection_t;

typedef struct {
    sb_graphics_t* graphics;
    sb_input_t* input;
    lua_State* L;
    bool is_running;
    bool console_mode;
    bool shutdown_confirmation;
    bool shutdown_selected;
    bool shutdown_test_reported;
    bool paused;
    pause_menu_selection_t pause_selection;
    bool blocked_buttons[SB_MAX_PLAYERS][SB_BUTTON_COUNT];
    bool guru_active;
    bool execution_limit_exceeded;
    const char *execution_stage; /* Borrowed label, valid during the protected call. */
    
    // Diagnostic state
    uint32_t error_code;
    char error_title[64];
    char error_msg[256];
    bool error_msg_truncated;
    char stack_trace[512];
    
    // Active Cartridge Data
    vcart_header_t current_cart;
    bool cart_loaded;
    bool cartridge_exit_requested;
    bool cart_initialized;
    const char *cartridge_path;  /* Borrowed from argv or the stable launcher list. */
    const char *startup_cartridge_path; /* argv-owned; survives session cleanup */
    const char *cartridge_directory; /* argv-owned; valid for host lifetime */
    launcher_cartridge_t cartridges[SB_LAUNCHER_MAX_CARTRIDGES];
    size_t cartridge_count;
    size_t cartridge_selection;
    size_t cartridge_rejected;
    size_t cartridge_omitted;
    bool cartridge_scan_errors;
    bool cartridge_launch_requested;
    bool cartridge_rescan_requested;
} sugarbee_engine_t;

static sugarbee_engine_t g_engine = {0};

void trigger_guru_meditation(uint32_t code, const char* title, const char* message, const char* trace);
void render_guru_meditation(void);
static void end_cartridge(void);
static void scan_cartridge_directory(const char *directory);

/* Count hooks run during Lua bytecode execution, not inside native calls. */
static void lua_instruction_limit(lua_State *L, lua_Debug *ar) {
    (void)ar;
    g_engine.execution_limit_exceeded = true;
    luaL_error(L, "Lua execution limit exceeded in %s", g_engine.execution_stage);
}

/* Stack: consumes function/arguments; leaves one error on failure, none on success.
   A caught limit error can return LUA_OK; execution_limit_exceeded still records it.
   Shutdown reports to the console itself rather than activating Guru. */
static int call_lua_with_limit(int nargs, const char *stage, int budget,
                               bool guru_on_limit) {
    g_engine.execution_limit_exceeded = false;
    g_engine.execution_stage = stage;
    lua_sethook(g_engine.L, lua_instruction_limit, LUA_MASKCOUNT, budget);

    int status = lua_pcall(g_engine.L, nargs, 0, 0);
    lua_sethook(g_engine.L, NULL, 0, 0);
    g_engine.execution_stage = NULL;

    if (g_engine.execution_limit_exceeded && guru_on_limit) {
        char message[128];
        snprintf(message, sizeof(message),
                 "Lua execution limit exceeded in %s", stage);
        trigger_guru_meditation(0x00000004, "LUA EXECUTION LIMIT", message, NULL);
    }
    return status;
}

// --- CARTRIDGE LOADER ROUTINE ---

bool load_virtual_cartridge(const char* filepath) {
    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        trigger_guru_meditation(0x00000001, "CARTRIDGE NOT FOUND", "Failed to open specified .vcart file from storage path.", NULL);
        return false;
    }

    // Step 1: Read and validate 64-byte binary header
    vcart_err_t err = vcart_parse_header(fp, &g_engine.current_cart);
    fclose(fp);

    if (err != VCART_OK) {
        char err_detail[128];
        snprintf(err_detail, sizeof(err_detail), "Header validation failed: %s", vcart_error_string(err));
        trigger_guru_meditation(0x00000002, "INVALID CARTRIDGE HEADER", err_detail, NULL);
        return false;
    }

    printf("[SugarBee] Virtual Cartridge Loaded Successfully!\n");
    printf("  Title:        %s\n", g_engine.current_cart.title);
    printf("  Payload Size: %u Bytes\n", g_engine.current_cart.payload_size);

    /* Extract the entry point with the same size and ZIP-CRC checks as assets. */
    size_t script_size = 0;
    uint8_t *main_lua_source = NULL;
    sb_asset_result_t asset_status = sb_asset_read(filepath, "main.lua",
                                                   &main_lua_source, &script_size);
    if (asset_status != SB_ASSET_OK) {
        trigger_guru_meditation(0x00000003, "CARTRIDGE ENTRY POINT",
                                sb_asset_error_string(asset_status), NULL);
        return false;
    }

    g_engine.cartridge_path = filepath;
    /* Compile from bytes with an explicit length, not a NUL-terminated string. */
    int lua_status = luaL_loadbuffer(g_engine.L, (const char *)main_lua_source,
                                     script_size, "@main.lua");
    free(main_lua_source);
    if (lua_status == LUA_OK) {
        lua_status = call_lua_with_limit(0, "main.lua",
                                        SB_LUA_STARTUP_INSTRUCTION_LIMIT, true);
    }
    if (lua_status != LUA_OK || g_engine.guru_active) {
        if (lua_status != LUA_OK) {
            if (!g_engine.guru_active) {
                const char* lua_err = lua_tostring(g_engine.L, -1);
                trigger_guru_meditation(0x00000004, "LUA CARTRIDGE ERROR", lua_err, NULL);
            }
            lua_pop(g_engine.L, 1);
        }
        return false;
    }
    g_engine.cart_loaded = true;
    return true;
}

// --- C LUA API BINDINGS ---

static int l_sugarbee_cls(lua_State* L) {
    int r = (int)luaL_optinteger(L, 1, 0);
    int g = (int)luaL_optinteger(L, 2, 0);
    int b = (int)luaL_optinteger(L, 3, 0);
    sb_graphics_clear(g_engine.graphics, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    return 0;
}

static int l_sugarbee_rect(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    int r = (int)luaL_optinteger(L, 5, 255);
    int g = (int)luaL_optinteger(L, 6, 255);
    int b = (int)luaL_optinteger(L, 7, 255);
    bool filled = lua_toboolean(L, 8);

    sb_graphics_rect(g_engine.graphics, x, y, w, h,
                     (uint8_t)r, (uint8_t)g, (uint8_t)b, filled);
    return 0;
}

static int l_sugarbee_print(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    int r = (int)luaL_optinteger(L, 4, 255);
    int g = (int)luaL_optinteger(L, 5, 255);
    int b = (int)luaL_optinteger(L, 6, 255);
    int scale = (int)luaL_optinteger(L, 7, 1);

    sb_graphics_text(g_engine.graphics, text, x, y,
                     (uint8_t)r, (uint8_t)g, (uint8_t)b, scale);
    return 0;
}

/* Upload packed RGBA pixels without going through a cartridge image decoder. */
static int l_sugarbee_texture_rgba(lua_State *L) {
    lua_Integer w = luaL_checkinteger(L, 1);
    lua_Integer h = luaL_checkinteger(L, 2);
    size_t data_size = 0;
    const char *data = luaL_checklstring(L, 3, &data_size);
    if (w <= 0 || w > INT_MAX || h <= 0 || h > INT_MAX)
        return luaL_error(L, "invalid texture dimensions");
    sb_texture_t texture = sb_graphics_texture_rgba(g_engine.graphics,
                                (int)w, (int)h,
                                (const uint8_t *)data, data_size);
    if (texture == SB_TEXTURE_INVALID)
        return luaL_error(L, "failed to create RGBA texture (check dimensions and buffer size)");
    lua_pushinteger(L, (lua_Integer)texture);
    return 1;
}

/* Load a PNG from the active cartridge, then upload it using the same
 * language-neutral texture service as texture_rgba(). */
static int l_sugarbee_texture_load(lua_State *L) {
    size_t path_length = 0;
    const char *path = luaL_checklstring(L, 1, &path_length);
    if (strlen(path) != path_length)
        return luaL_error(L, "asset path contains a NUL byte");
    if (!g_engine.cartridge_path)
        return luaL_error(L, "no active cartridge");
    uint8_t *encoded = NULL;
    size_t encoded_length = 0;
    sb_asset_result_t result = sb_asset_read(g_engine.cartridge_path, path,
                                              &encoded, &encoded_length);
    if (result != SB_ASSET_OK)
        return luaL_error(L, "cannot load asset '%s': %s", path,
                          sb_asset_error_string(result));

    uint8_t *rgba = NULL;
    int width = 0, height = 0;
    size_t rgba_length = 0;
    bool valid_png = sb_png_decode_rgba(encoded, encoded_length,
                                        &rgba, &width, &height, &rgba_length);
    free(encoded);
    if (!valid_png)
        return luaL_error(L, "invalid or oversized PNG asset '%s'", path);

    sb_texture_t texture = sb_graphics_texture_rgba(g_engine.graphics,
                                                     width, height, rgba,
                                                     rgba_length);
    free(rgba);
    if (texture == SB_TEXTURE_INVALID)
        return luaL_error(L, "failed to create texture for '%s'", path);
    lua_pushinteger(L, (lua_Integer)texture);
    return 1;
}

static int l_sugarbee_sprite(lua_State *L) {
    lua_Integer texture = luaL_checkinteger(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    if (texture <= 0 || (uint64_t)texture > UINT32_MAX ||
        !sb_graphics_sprite(g_engine.graphics, (sb_texture_t)texture, x, y))
        return luaL_error(L, "invalid texture handle or sprite drawing failed");
    return 0;
}

/* Additional 3C sprite features stay in the same SDL-free C graphics API.
 * Keep sprite(handle, x, y) unchanged for existing cartridges. */
static int sprite_int_arg(lua_State *L, int arg) {
    lua_Integer value = luaL_checkinteger(L, arg);
    if (value < INT_MIN || value > INT_MAX)
        luaL_argerror(L, arg, "integer outside supported range");
    return (int)value;
}

static int l_sugarbee_sprite_region(lua_State *L) {
    lua_Integer handle = luaL_checkinteger(L, 1);
    int x = sprite_int_arg(L, 2);
    int y = sprite_int_arg(L, 3);
    int src_x = sprite_int_arg(L, 4);
    int src_y = sprite_int_arg(L, 5);
    int src_w = sprite_int_arg(L, 6);
    int src_h = sprite_int_arg(L, 7);
    int dest_w = lua_isnoneornil(L, 8) ? src_w : sprite_int_arg(L, 8);
    int dest_h = lua_isnoneornil(L, 9) ? src_h : sprite_int_arg(L, 9);
    bool flip_x = lua_toboolean(L, 10);
    bool flip_y = lua_toboolean(L, 11);
    if (handle <= 0 || (uint64_t)handle > UINT32_MAX ||
        !sb_graphics_sprite_region(g_engine.graphics, (sb_texture_t)handle,
                                    x, y, src_x, src_y, src_w, src_h,
                                    dest_w, dest_h, flip_x, flip_y))
        return luaL_error(L, "invalid texture handle, sprite region, or draw operation");
    return 0;
}

static int l_sugarbee_texture_free(lua_State *L) {
    lua_Integer texture = luaL_checkinteger(L, 1);
    lua_pushboolean(L, texture > 0 && (uint64_t)texture <= UINT32_MAX &&
        sb_graphics_texture_destroy(g_engine.graphics, (sb_texture_t)texture));
    return 1;
}

static sb_button_t button_from_name(const char *name) {
    if (strcmp(name, "up") == 0) return SB_BUTTON_UP;
    if (strcmp(name, "down") == 0) return SB_BUTTON_DOWN;
    if (strcmp(name, "left") == 0) return SB_BUTTON_LEFT;
    if (strcmp(name, "right") == 0) return SB_BUTTON_RIGHT;
    if (strcmp(name, "a") == 0) return SB_BUTTON_A;
    if (strcmp(name, "b") == 0) return SB_BUTTON_B;
    if (strcmp(name, "x") == 0) return SB_BUTTON_X;
    if (strcmp(name, "y") == 0) return SB_BUTTON_Y;
    if (strcmp(name, "start") == 0) return SB_BUTTON_START;
    if (strcmp(name, "select") == 0) return SB_BUTTON_SELECT;
    return SB_BUTTON_INVALID;
}

/* Whether this button may currently be exposed to the cartridge. */
static bool cartridge_button_allowed(int player, sb_button_t button) {
    if (player < 0 || player >= SB_MAX_PLAYERS ||
        button < 0 || button >= SB_BUTTON_COUNT) {
            return false;
        }

    return !g_engine.paused &&
        button != SB_BUTTON_START &&
        !g_engine.blocked_buttons[player][button];
}

static int l_sugarbee_btn(lua_State* L) {
    const char *name = luaL_checkstring(L, 1);
    int player = (int)luaL_optinteger(L, 2, 0);
    sb_button_t button = button_from_name(name);

    lua_pushboolean(L, cartridge_button_allowed(player, button) &&
        sb_input_down(g_engine.input, player, button));
    return 1;
}

/* True for one host update when a button changes from up to down. */
static int l_sugarbee_btnp(lua_State* L) {
    const char *name = luaL_checkstring(L, 1);
    int player = (int)luaL_optinteger(L, 2, 0);
    sb_button_t button = button_from_name(name);

    lua_pushboolean(L, cartridge_button_allowed(player, button) &&
        sb_input_pressed(g_engine.input, player, button));
    return 1;
}

void register_sugarbee_api(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_sugarbee_cls);   lua_setfield(L, -2, "cls");
    lua_pushcfunction(L, l_sugarbee_rect);  lua_setfield(L, -2, "rect");
    lua_pushcfunction(L, l_sugarbee_print); lua_setfield(L, -2, "print");
    lua_pushcfunction(L, l_sugarbee_texture_rgba); lua_setfield(L, -2, "texture_rgba");
    lua_pushcfunction(L, l_sugarbee_texture_load); lua_setfield(L, -2, "texture_load");
    lua_pushcfunction(L, l_sugarbee_sprite); lua_setfield(L, -2, "sprite");
    lua_pushcfunction(L, l_sugarbee_sprite_region); lua_setfield(L, -2, "sprite_region");
    lua_pushcfunction(L, l_sugarbee_texture_free); lua_setfield(L, -2, "texture_free");
    lua_pushcfunction(L, l_sugarbee_btn);   lua_setfield(L, -2, "btn");
    lua_pushcfunction(L, l_sugarbee_btnp);  lua_setfield(L, -2, "btnp");
    lua_setglobal(L, "sugarbee");
}

// --- TEXT RENDERER & GURU MEDITATION ---

void trigger_guru_meditation(uint32_t code, const char* title, const char* message, const char* trace) {
    fprintf(stderr, "\n[SugarBee] SOFTWARE FAILURE #%08X\n", code);
    fprintf(stderr, "%s\n", title ? title : "(no title)");
    fprintf(stderr, "%s\n", message ? message : "(no message)");

    if (trace && trace[0] != '\0') {
        fprintf(stderr, "%s\n", trace);
    }

    fflush(stderr);
    g_engine.guru_active = true;
    g_engine.error_code = code;
    strncpy(g_engine.error_title, title, sizeof(g_engine.error_title) - 1);
    const char *display_message = message ? message : "(no message)";

    g_engine.error_msg_truncated = strlen(display_message) >= sizeof(g_engine.error_msg);

    snprintf(g_engine.error_msg, sizeof(g_engine.error_msg), "%s", display_message);
    if (trace) strncpy(g_engine.stack_trace, trace, sizeof(g_engine.stack_trace) - 1);
}

/* Returns true if some text could not fit on screen. */
static bool draw_guru_message(const char *text) {
    enum { columns = 72, max_lines = 19 };
    char line[columns + 1];
    const char *cursor = text;
    int y = 200;

    for (int row = 0; row < max_lines && *cursor; ++row) {
        size_t count = 0;

        /* Find the end of this line or the available width. */
        while (count < columns &&
               cursor[count] != '\0' &&
               cursor[count] != '\n') {
            ++count;
        }

        /* Prefer a word boundary when more text follows. */
        if (count == columns &&
            cursor[count] != '\0' &&
            cursor[count] != '\n' &&
            cursor[count] != ' ') {
            size_t split = count;

            while (split > 0 && cursor[split - 1] != ' ') {
                --split;
            }

            if (split > 0) {
                count = split;
            }
        }

        memcpy(line, cursor, count);
        line[count] = '\0';

        sb_graphics_text(g_engine.graphics, line,
                         60, y, 200, 200, 200, 2);
        y += 20;
        cursor += count;

        /* Discard spaces at the wrap boundary. */
        while (*cursor == ' ') {
            ++cursor;
        }

        if (*cursor == '\n') {
            ++cursor;
        }
    }

    return *cursor != '\0';
}

void render_guru_meditation(void) {
    sb_graphics_clear(g_engine.graphics, 0, 0, 0);

    static uint32_t frame = 0;
    bool flash = ((frame++ / 30) % 2 == 0);

    int width = sb_graphics_width(g_engine.graphics);
    int height = sb_graphics_height(g_engine.graphics);
    for (int i = 0; i < 6; i++) {
        sb_graphics_rect(g_engine.graphics, 20 + i, 20 + i,
                         width - 40 - (2 * i), height - 40 - (2 * i),
                         flash ? 255 : 128, 0, 0, false);
    }

    char code_str[64];
    snprintf(code_str, sizeof(code_str), "SOFTWARE FAILURE #%08X", g_engine.error_code);

    sb_graphics_text(g_engine.graphics, "GURU MEDITATION V2", 60, 60, 255, 0, 0, 4);
    sb_graphics_text(g_engine.graphics, code_str, 60, 110, 255, 0, 0, 3);
    sb_graphics_text(g_engine.graphics, g_engine.error_title, 60, 160, 255, 255, 255, 2);
    bool message_overflow = draw_guru_message(g_engine.error_msg);

    if (g_engine.error_msg_truncated || message_overflow) {
        sb_graphics_text(g_engine.graphics, "Message continues in console", 60, 600, 255, 215, 100, 2);
    }

    sb_graphics_text(g_engine.graphics,
        "B / K: Return to host",
        60, 640, 150, 150, 150, 2);
}

/* A Lua callback may optionally receive the fixed simulation delta. */
void call_lua_global_function(const char* func_name, bool with_dt, double dt) {
    if (!g_engine.L || g_engine.guru_active) return;

    lua_getglobal(g_engine.L, func_name);
    if (lua_isfunction(g_engine.L, -1)) {
        if (with_dt) lua_pushnumber(g_engine.L, dt);
        int status;
        if (strcmp(func_name, "update") == 0) {
            status = call_lua_with_limit(with_dt ? 1 : 0, "update()",
                                         SB_LUA_UPDATE_INSTRUCTION_LIMIT, true);
        } else if (strcmp(func_name, "draw") == 0) {
            status = call_lua_with_limit(with_dt ? 1 : 0, "draw()",
                                         SB_LUA_DRAW_INSTRUCTION_LIMIT, true);
        } else if (strcmp(func_name, "init") == 0) {
            status = call_lua_with_limit(with_dt ? 1 : 0, "init()",
                                         SB_LUA_STARTUP_INSTRUCTION_LIMIT, true);
        } else {
            status = lua_pcall(g_engine.L, with_dt ? 1 : 0, 0, 0);
        }

        if (status != LUA_OK && !g_engine.guru_active) {
            const char* err = lua_tostring(g_engine.L, -1);
            trigger_guru_meditation(0x00000004, "LUA RUNTIME EXCEPTION", err, NULL);
        }
        if (status != LUA_OK) {
            lua_pop(g_engine.L, 1);
        }
    } else {
        lua_pop(g_engine.L, 1);
    }
}

static bool start_cartridge(const char *filepath) {
    /* A new session must not overwrite an existing Lua state. */
    if (!filepath || g_engine.L) return false;

    g_engine.L = luaL_newstate();
    if (!g_engine.L) {
        trigger_guru_meditation(
            0x00000004,
            "LUA INITIALIZATION FAILED",
            "Could not create a Lua state for the cartridge.",
            NULL);
        return false;
    }

    luaL_openlibs(g_engine.L);
    register_sugarbee_api(g_engine.L);

    if (!load_virtual_cartridge(filepath)) {
        end_cartridge();
        return false;
    }

    call_lua_global_function("init", false, 0.0);
    g_engine.cart_initialized = !g_engine.guru_active;

    if (!g_engine.cart_initialized) {
        end_cartridge();
        return false;
    }

    return true;
}

bool init_engine(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) return false;

    g_engine.graphics = sb_graphics_create("SugarBee Console Host",
                                            SB_DEFAULT_WIDTH, SB_DEFAULT_HEIGHT);
    if (!g_engine.graphics) return false;

    g_engine.input = sb_input_create();
    if (!g_engine.input) return false;

    g_engine.is_running = true;
    return true;
}

static void draw_cartridge_list(void) {
    sb_graphics_clear(g_engine.graphics, 20, 20, 20);

    sb_graphics_text(g_engine.graphics,
                     "SugarBee - Cartridges", 80, 50,
                     255, 220, 80, 3);

    size_t first = 0;

    if (g_engine.cartridge_selection >= 8) {
        first = g_engine.cartridge_selection - 7;
    }

    size_t visible = g_engine.cartridge_count - first;
    if (visible > 8) visible = 8;

    char summary[96];
    snprintf(summary, sizeof(summary),
             "Selected %zu of %zu",
             g_engine.cartridge_count > 0
                 ? g_engine.cartridge_selection + 1 : 0,
             g_engine.cartridge_count);

    sb_graphics_text(g_engine.graphics, summary, 80, 100,
                     200, 200, 200, 2);

    if (visible == 0) {
        const char *message = g_engine.cartridge_scan_errors
            ? "No cartridges listed: directory scan had errors."
            : "No eligible cartridges found.";

        sb_graphics_text(g_engine.graphics, message, 80, 160,
                         255, 255, 255, 2);
    }

    for (size_t i = 0; i < visible; ++i) {
        size_t index = first + i;
        const launcher_cartridge_t *item = &g_engine.cartridges[index];

        char title[32];
        snprintf(title, sizeof(title), "%s",
                 item->title[0] ? item->title : "(Untitled)");

        const char *filename = strrchr(item->path, '/');
        filename = filename ? filename + 1 : item->path;

        char label[96];
        snprintf(label, sizeof(label), "%.90s%s",
                 filename, strlen(filename) > 90 ? "..." : "");

        /* Keep each label on one line using the supported font characters. */
        for (size_t j = 0; title[j] != '\0'; ++j) {
            unsigned char c = (unsigned char)title[j];
            if (c < 32 || c > 126) title[j] = '?';
        }

        for (size_t j = 0; label[j] != '\0'; ++j) {
            unsigned char c = (unsigned char)label[j];
            if (c < 32 || c > 126) label[j] = '?';
        }

        int y = 145 + (int)i * 58;

        if (index == g_engine.cartridge_selection) {
            sb_graphics_text(g_engine.graphics, ">", 48, y,
                             255, 220, 80, 2);
        }

        sb_graphics_text(g_engine.graphics, title, 80, y,
                         255, 255, 255, 2);
        sb_graphics_text(g_engine.graphics, label, 80, y + 25,
                         170, 170, 170, 1);
    }
    char scan_summary[128];
    snprintf(scan_summary, sizeof(scan_summary),
             "Launcher capacity: %d cartridges. "
             "Additional not listed: %zu. Rejected: %zu.",
             SB_LAUNCHER_MAX_CARTRIDGES,
             g_engine.cartridge_omitted,
             g_engine.cartridge_rejected);

    sb_graphics_text(g_engine.graphics, scan_summary, 80, 610,
                     200, 200, 200, 1);

    if (g_engine.cartridge_scan_errors) {
        sb_graphics_text(g_engine.graphics,
                         "Directory scan had errors. See console for details.",
                         80, 628, 255, 220, 80, 1);
    } else if (g_engine.cartridge_rejected > 0 ||
               g_engine.cartridge_omitted > 0) {
        sb_graphics_text(g_engine.graphics,
                         "Some files are not listed. See console for details.",
                         80, 628, 255, 220, 80, 1);
    }
    sb_graphics_text(g_engine.graphics,
                     g_engine.console_mode
                         ? "UP/DOWN: Select  A/J: Launch  X/U: Rescan  START: Shutdown test"
                         : "UP/DOWN: Select  A/J: Launch  X/U: Rescan  B/K: Close",
                     80, 650,
                     255, 255, 255, 2);
}

/* Called with press edges, never held-state input. Returns true on dialog transitions. */
static bool handle_idle_host_input(bool start, bool back, bool up, bool down,
                                   bool confirm, bool rescan) {
    if (g_engine.cart_loaded || g_engine.guru_active) return false;

    if (g_engine.console_mode && g_engine.shutdown_confirmation) {
        if (back || start) {
            g_engine.shutdown_confirmation = false;
            return true;
        }
        if (confirm) {
            if (g_engine.shutdown_selected) {
                /* Test action only: no OS command, cleanup, or host exit. */
                printf("[SugarBee] Shutdown test requested; no power-off performed.\n");
                fflush(stdout);
                g_engine.shutdown_test_reported = true;
            }
            g_engine.shutdown_confirmation = false;
            return true;
        }
        if (up && !down) g_engine.shutdown_selected = false;
        else if (down && !up) g_engine.shutdown_selected = true;
        return false;
    }

    if (g_engine.console_mode && start) {
        g_engine.shutdown_confirmation = true;
        g_engine.shutdown_selected = false;
        g_engine.shutdown_test_reported = false;
        return true; /* The opening press cannot confirm or move selection. */
    }
    if (back) {
        if (!g_engine.console_mode) g_engine.is_running = false;
    } else if (g_engine.cartridge_directory) {
        if (rescan) {
            g_engine.cartridge_rescan_requested = true;
        } else if (g_engine.cartridge_count > 0) {
            if (confirm) g_engine.cartridge_launch_requested = true;
            else if (up && !down && g_engine.cartridge_selection > 0)
                --g_engine.cartridge_selection;
            else if (down && !up &&
                     g_engine.cartridge_selection + 1 < g_engine.cartridge_count)
                ++g_engine.cartridge_selection;
        }
    } else if (confirm && g_engine.startup_cartridge_path) {
        g_engine.cartridge_launch_requested = true;
    }
    return false;
}

static void draw_shutdown_confirmation(void) {
    sb_graphics_rect(g_engine.graphics, 320, 190, 640, 320, 20, 20, 20, true);
    sb_graphics_text(g_engine.graphics, "Shut down console?", 380, 220,
                     255, 220, 80, 2);
    sb_graphics_text(g_engine.graphics, "TEST ONLY - no power-off", 380, 260,
                     255, 220, 80, 2);
    sb_graphics_text(g_engine.graphics, "Cancel", 420, 325, 255, 255, 255, 2);
    sb_graphics_text(g_engine.graphics, "Shut down (test)", 420, 370,
                     255, 255, 255, 2);
    sb_graphics_text(g_engine.graphics, ">", 380,
                     g_engine.shutdown_selected ? 370 : 325, 255, 220, 80, 2);
    sb_graphics_text(g_engine.graphics,
                     "UP/DOWN: Select  A/J: Confirm  B/K or START: Cancel",
                     360, 455, 200, 200, 200, 1);
}

void main_loop(void) {
    sb_timing_t timing;
    sb_timing_reset(&timing);
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 last_counter = SDL_GetPerformanceCounter();

    while (g_engine.is_running) {
        if (!sb_input_poll(g_engine.input)) {
            g_engine.is_running = false;
            break;
        }

                /* Guru has its own input handling while game updates are stopped. */
        if (g_engine.guru_active) {
            bool recover_pressed = false;

            sb_input_begin_update(g_engine.input);

            for (int player = 0; player < SB_MAX_PLAYERS; ++player) {
                if (sb_input_pressed(g_engine.input, player, SB_BUTTON_B)) {
                    recover_pressed = true;
                }

                for (int button = 0; button < SB_BUTTON_COUNT; ++button) {
                    g_engine.blocked_buttons[player][button] =
                        sb_input_down(g_engine.input, player,
                                      (sb_button_t)button) ||
                        sb_input_pressed(g_engine.input, player,
                                         (sb_button_t)button);
                }
            }

            sb_input_end_update(g_engine.input);

            if (recover_pressed) {
                /* Keep Guru active during cleanup to skip Lua shutdown. */
                end_cartridge();

                g_engine.cartridge_launch_requested = false;
                g_engine.guru_active = false;

                sb_timing_reset(&timing);
                last_counter = SDL_GetPerformanceCounter();

                /* Do not reuse the recovery input as idle-host input. */
                continue;
            }
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        double elapsed = 0.0;
        if (frequency != 0 && now >= last_counter)
            elapsed = (double)(now - last_counter) / (double)frequency;
        last_counter = now;

        unsigned updates = 0;
        if (!g_engine.guru_active) {
            updates = sb_timing_advance(&timing, elapsed);
            for (unsigned i = 0; i < updates; ++i) {
                /* Only the first update after a press sees btnp == true. */
                sb_input_begin_update(g_engine.input);

                bool start_pressed = false;
                bool back_pressed = false;
                bool up_pressed = false;
                bool down_pressed = false;
                bool confirm_pressed = false;
                bool rescan_pressed = false;

                /*  Player Loop
                    Start, B, Up, Down, A
                */
                for (int player = 0; player < SB_MAX_PLAYERS; ++player) {
                    if (sb_input_pressed(g_engine.input, player, SB_BUTTON_START)) {
                        start_pressed = true;
                    }
                    if (sb_input_pressed(g_engine.input, player, SB_BUTTON_B)) {
                        back_pressed = true;
                    }
                    if (sb_input_pressed(g_engine.input, player, SB_BUTTON_UP)) {
                        up_pressed = true;
                    }
                    if (sb_input_pressed(g_engine.input, player, SB_BUTTON_DOWN)) {
                        down_pressed = true;
                    }
                    if (sb_input_pressed(g_engine.input, player, SB_BUTTON_A)) {
                        confirm_pressed = true;
                    }
                    if (sb_input_pressed(g_engine.input, player, SB_BUTTON_X)) {
                        rescan_pressed = true;
                    }
                }

                bool pause_changed = false;

                bool shutdown_dialog_changed = handle_idle_host_input(
                    start_pressed, back_pressed, up_pressed, down_pressed,
                    confirm_pressed, rescan_pressed);

                /* Pause toggle block */
                if (g_engine.cart_loaded && start_pressed) {
                    g_engine.paused = !g_engine.paused;
                    pause_changed = true;

                    if (g_engine.paused) {
                        g_engine.pause_selection = PAUSE_MENU_RESUME;
                    }
                } else if (g_engine.paused && back_pressed) {
                    g_engine.paused = false;
                    pause_changed = true;
                }

                /* Handle menu actions only when already paused */
                if (g_engine.paused && !pause_changed) {
                    if (confirm_pressed) {
                        if (g_engine.pause_selection == PAUSE_MENU_RESUME) {
                            g_engine.paused = false;
                            pause_changed = true;
                        } else if (g_engine.pause_selection == PAUSE_MENU_EXIT) {
                            g_engine.cartridge_exit_requested = true;
                        }
                    } else if (up_pressed && !down_pressed) {
                        g_engine.pause_selection = PAUSE_MENU_RESUME;
                    } else if (down_pressed && !up_pressed) {
                        g_engine.pause_selection = PAUSE_MENU_EXIT;
                    }
                }

                /* Keep menu inputs hidden until released. */
                for (int player = 0; player < SB_MAX_PLAYERS; ++player) {
                    for (int button = 0; button < SB_BUTTON_COUNT; ++button) {
                        bool down = sb_input_down(g_engine.input, player, (sb_button_t)button);
                        bool pressed = sb_input_pressed(g_engine.input, player, (sb_button_t)button);

                        if (g_engine.paused || pause_changed ||
                            g_engine.shutdown_confirmation || shutdown_dialog_changed ||
                            g_engine.cartridge_launch_requested ||
                            g_engine.cartridge_rescan_requested) {
                            g_engine.blocked_buttons[player][button] = down || pressed;
                        } else if (!down && !pressed) {
                            g_engine.blocked_buttons[player][button] = false;
                        }
                    }
                }
                if (g_engine.cart_loaded && !g_engine.paused && !pause_changed) {
                    call_lua_global_function(
                        "update", true, sb_timing_step_seconds());
                }

                sb_input_end_update(g_engine.input);

                if (!g_engine.is_running || g_engine.guru_active ||
                    g_engine.cartridge_exit_requested ||
                    g_engine.cartridge_launch_requested ||
                    g_engine.cartridge_rescan_requested) {
                    break;
                }
            }
        }

        if (!g_engine.is_running) break;

        if (g_engine.cartridge_exit_requested) {
            end_cartridge();
            sb_timing_reset(&timing);
            last_counter = SDL_GetPerformanceCounter();
        } else if (g_engine.cartridge_rescan_requested) {
            g_engine.cartridge_rescan_requested = false;

            if (!g_engine.cart_loaded && g_engine.cartridge_directory) {
                scan_cartridge_directory(g_engine.cartridge_directory);
            }

            sb_timing_reset(&timing);
            last_counter = SDL_GetPerformanceCounter();
        } else if (g_engine.cartridge_launch_requested) {
            g_engine.cartridge_launch_requested = false;

            const char *path = g_engine.startup_cartridge_path;

            if (g_engine.cartridge_directory) {
                path = NULL;

                if (g_engine.cartridge_selection < g_engine.cartridge_count) {
                    path = g_engine.cartridges[
                        g_engine.cartridge_selection].path;
                }
            }

            if (path) {
                start_cartridge(path);
            }

            sb_timing_reset(&timing);
            last_counter = SDL_GetPerformanceCounter();
        }

        if (g_engine.guru_active) {
            render_guru_meditation();
        } else if (!g_engine.cart_loaded && g_engine.cartridge_directory) {
            draw_cartridge_list();
        } else if (!g_engine.cart_loaded) {
            sb_graphics_clear(g_engine.graphics, 20, 20, 20);
            sb_graphics_text(g_engine.graphics,
                             "SugarBee", 512, 260,
                             255, 220, 80, 4);
            sb_graphics_text(g_engine.graphics,
                             "No cartridge running", 480, 340,
                             255, 255, 255, 2);
            if (g_engine.startup_cartridge_path) {
                sb_graphics_text(g_engine.graphics,
                                 "A / J: Relaunch cartridge", 448, 390,
                                 255, 255, 255, 2);
            } else {
                sb_graphics_text(g_engine.graphics,
                                 "Launch: sugarbee path/to/game.vcart",
                                 376, 390,
                                 255, 255, 255, 2);
            }

            sb_graphics_text(g_engine.graphics,
                             g_engine.console_mode
                                 ? "START / ENTER: Shutdown test"
                                 : "B / K: Close SugarBee",
                             g_engine.console_mode ? 400 : 480, 430,
                             255, 255, 255, 2);
        } else {
            if (!g_engine.paused) {
                if (!sb_graphics_begin_game_frame(g_engine.graphics)) {
                    SDL_Log("SugarBee: could not begin game frame: %s", SDL_GetError());
                    g_engine.is_running = false;
                    break;
                }

                call_lua_global_function("draw", false, 0.0);
            }

            if (!sb_graphics_show_game_frame(g_engine.graphics)) {
                SDL_Log("SugarBee: could not display game frame: %s", SDL_GetError());
                g_engine.is_running = false;
                break;
            }

            if (g_engine.guru_active) {
                render_guru_meditation();
            } else if (g_engine.paused) {
                sb_graphics_rect(g_engine.graphics, 400, 260, 480, 220, 20, 20, 20, true);
                sb_graphics_text(g_engine.graphics, "PAUSED", 544, 285, 255, 255, 255, 2);
                sb_graphics_text(g_engine.graphics, "Resume", 520, 345, 255, 255, 255, 2);
                sb_graphics_text(g_engine.graphics, "Exit Game", 520, 390, 255, 255, 255, 2);

                int marker_y = g_engine.pause_selection == PAUSE_MENU_RESUME ? 345 : 390;

                sb_graphics_text(g_engine.graphics, ">", 480, marker_y, 255, 220, 80, 2);
            }
        }

        if (g_engine.console_mode && !g_engine.cart_loaded && !g_engine.guru_active) {
            if (g_engine.shutdown_confirmation) {
                draw_shutdown_confirmation();
            } else if (g_engine.shutdown_test_reported) {
                sb_graphics_text(g_engine.graphics,
                                 "Shutdown test complete - no power-off", 80, 690,
                                 255, 220, 80, 2);
            }
        }

        sb_graphics_present(g_engine.graphics);
        /* Software-renderer fallback may not have VSync; don't spin idle. */
        if (!g_engine.guru_active && updates == 0) SDL_Delay(1);
    }
}

/* Stack: unchanged on return; no arguments or results. */
static void call_lua_shutdown(void) {
    if (!g_engine.L || !g_engine.cart_initialized || g_engine.guru_active) {
        return;
    }

    /* Consume eligibility before calling Lua: invoke at most once. */
    g_engine.cart_initialized = false;

    lua_getglobal(g_engine.L, "shutdown");
    if (!lua_isfunction(g_engine.L, -1)) {
        lua_pop(g_engine.L, 1);
        return;
    }

    int status = call_lua_with_limit(0, "shutdown()",
                                     SB_LUA_SHUTDOWN_INSTRUCTION_LIMIT, false);
    if (g_engine.execution_limit_exceeded) {
        fprintf(stderr,
                "Lua shutdown error: Lua execution limit exceeded in shutdown()\n");
    } else if (status != LUA_OK) {
        const char *err = lua_tostring(g_engine.L, -1);
        fprintf(stderr, "Lua shutdown error: %s\n",
                err ? err : "(non-string error)");
    }
    if (status != LUA_OK) {
        lua_pop(g_engine.L, 1);
    }
}

static void end_cartridge(void) {
    call_lua_shutdown();

    if (g_engine.L) {
        lua_close(g_engine.L);
        g_engine.L = NULL;
    }

    sb_graphics_clear_cartridge_textures(g_engine.graphics);

    g_engine.cart_initialized = false;
    g_engine.cart_loaded = false;
    g_engine.cartridge_path = NULL;
    g_engine.cartridge_exit_requested = false;
    g_engine.paused = false;
    g_engine.pause_selection = PAUSE_MENU_RESUME;
}

void shutdown_engine(void) {
    end_cartridge();

    sb_input_destroy(g_engine.input);
    sb_graphics_destroy(g_engine.graphics);
    SDL_Quit();
}

static bool inspect_cartridge(const char *path, vcart_header_t *header) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "[SugarBee] Rejected '%s': %s\n",
                path, strerror(errno));
        return false;
    }

    vcart_err_t header_status = vcart_parse_header(file, header);
    fclose(file);

    if (header_status != VCART_OK) {
        fprintf(stderr, "[SugarBee] Rejected '%s': %s\n",
                path, vcart_error_string(header_status));
        return false;
    }

    uint8_t *script = NULL;
    size_t script_size = 0;

    sb_asset_result_t asset_status =
        sb_asset_read(path, "main.lua", &script, &script_size);

    free(script);

    if (asset_status != SB_ASSET_OK) {
        fprintf(stderr, "[SugarBee] Rejected '%s': %s\n",
                path, sb_asset_error_string(asset_status));
        return false;
    }

    return true;
}

static int compare_cartridges(const void *left, const void *right) {
    const launcher_cartridge_t *a = left;
    const launcher_cartridge_t *b = right;

    int title_order = strcmp(a->title, b->title);
    if (title_order != 0) {
        return title_order;
    }

    return strcmp(a->path, b->path);
}

static void scan_cartridge_directory(const char *directory) {
    g_engine.cartridge_count = 0;
    g_engine.cartridge_selection = 0;
    g_engine.cartridge_rejected = 0;
    g_engine.cartridge_omitted = 0;
    g_engine.cartridge_scan_errors = false;
    DIR *dir = opendir(directory);
    if (!dir) {
        fprintf(stderr, "[SugarBee] Cannot open directory '%s': %s\n",
                directory, strerror(errno));
                g_engine.cartridge_scan_errors = true;
        return;
    }

    size_t eligible = 0;
    size_t rejected = 0;
    bool scan_failed = false;

    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);

        if (!entry) {
            if (errno != 0) {
                fprintf(stderr, "[SugarBee] Cannot finish scanning '%s': %s\n",
                        directory, strerror(errno));
                scan_failed = true;
            }
            break;
        }

        size_t length = strlen(entry->d_name);
        if (length < 6 ||
            strcmp(entry->d_name + length - 6, ".vcart") != 0) {
            continue;
        }

        char path[SB_LAUNCHER_PATH_SIZE];
        int written = snprintf(path, sizeof(path), "%s/%s",
                               directory, entry->d_name);

        if (written < 0 || (size_t)written >= sizeof(path)) {
            fprintf(stderr, "[SugarBee] Skipping '%s': path too long\n",
                    entry->d_name);
                    scan_failed = true;
            continue;
        }

        struct stat info;
        if (lstat(path, &info) != 0) {
            fprintf(stderr, "[SugarBee] Cannot inspect '%s': %s\n",
                    path, strerror(errno));
                    scan_failed = true;
            continue;
        }

        if (!S_ISREG(info.st_mode)) continue;

        vcart_header_t header = {0};

        if (inspect_cartridge(path, &header)) {
            printf("[SugarBee] Eligible: %s (%s)\n",
                   header.title, entry->d_name);
            ++eligible;

            if (g_engine.cartridge_count < SB_LAUNCHER_MAX_CARTRIDGES) {
                launcher_cartridge_t *item =
                    &g_engine.cartridges[g_engine.cartridge_count];

                memcpy(item->title, header.title, sizeof(item->title));
                memcpy(item->path, path, (size_t)written + 1);

                ++g_engine.cartridge_count;
            }
        } else {
            ++rejected;
        }
    }

    if (closedir(dir) != 0) {
        fprintf(stderr, "[SugarBee] Cannot close directory '%s': %s\n",
                directory, strerror(errno));
                scan_failed = true;
    }

    g_engine.cartridge_rejected = rejected;
    g_engine.cartridge_omitted = eligible - g_engine.cartridge_count;
    g_engine.cartridge_scan_errors = scan_failed;
    qsort(g_engine.cartridges,
          g_engine.cartridge_count,
          sizeof(g_engine.cartridges[0]),
          compare_cartridges);

    printf("[SugarBee] %zu eligible, %zu rejected%s.\n",
           eligible, rejected,
           scan_failed ? " (scan errors; see console)" : "");

    printf("[SugarBee] Stored %zu cartridge(s) for the launcher.\n",
           g_engine.cartridge_count);

    if (eligible > g_engine.cartridge_count) {
        fprintf(stderr,
                "[SugarBee] Launcher capacity reached: "
                "%zu eligible cartridge(s) were not stored.\n",
                eligible - g_engine.cartridge_count);
    }

    for (size_t i = 0; i < g_engine.cartridge_count; ++i) {
        printf("[SugarBee] List %zu: %s [%s]\n",
               i + 1,
               g_engine.cartridges[i].title,
               g_engine.cartridges[i].path);
    }
}

/* --console is an explicit leading option; neither hardware nor display implies it. */
static bool parse_host_arguments(int argc, char *argv[]) {
    int first = 1;
    if (argc > first && strcmp(argv[first], "--console") == 0) {
        g_engine.console_mode = true;
        ++first;
    }
    int remaining = argc - first;
    if (remaining == 0) return true;
    if (remaining == 1 && argv[first][0] != '\0' && argv[first][0] != '-') {
        g_engine.startup_cartridge_path = argv[first];
        return true;
    }
    if (remaining == 2 && strcmp(argv[first], "--cartridges") == 0 &&
        argv[first + 1][0] != '\0' && argv[first + 1][0] != '-') {
        g_engine.cartridge_directory = argv[first + 1];
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    if (!parse_host_arguments(argc, argv)) {
        fprintf(stderr,
                "Usage: %s [--console] [cartridge.vcart]\n"
                "       %s [--console] --cartridges DIRECTORY\n",
                argv[0], argv[0]);
        return 1;
    }

    if (!init_engine()) {
        shutdown_engine();
        return 1;
    }

    if (g_engine.startup_cartridge_path) {
        start_cartridge(g_engine.startup_cartridge_path);
    } else if (g_engine.cartridge_directory) {
        printf("[SugarBee] Cartridge directory: %s\n",
               g_engine.cartridge_directory);
        scan_cartridge_directory(g_engine.cartridge_directory);
        if (g_engine.cartridge_count == 1 &&
            g_engine.cartridge_omitted == 0 &&
            !g_engine.cartridge_scan_errors) {
            start_cartridge(g_engine.cartridges[0].path);
        }
    }

    main_loop();
    shutdown_engine();
    return 0;
}