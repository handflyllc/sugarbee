/*
 * PROJECT SUGARBEE - C Host Engine
 * Target Platform: Raspberry Pi 4 (DRM/KMS / Linux Desktop)
 * Language: C99 / SDL2 / Embedded Lua 5.4 / zlib & minizip
 */

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

/* Cartridge asset readers and PNG decoding are outside the Lua adapter. */
#include "sb_assets.h"
#include "sb_png.h"

#include "vcart.h"
#include "sb_graphics.h"
#include "sb_input.h"
#include "sb_timing.h"

typedef enum {
    PAUSE_MENU_RESUME,
    PAUSE_MENU_EXIT
} pause_menu_selection_t;

typedef struct {
    sb_graphics_t* graphics;
    sb_input_t* input;
    lua_State* L;
    bool is_running;
    bool paused;
    pause_menu_selection_t pause_selection;
    bool blocked_buttons[SB_MAX_PLAYERS][SB_BUTTON_COUNT];
    bool guru_active;
    
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
    const char *cartridge_path;  /* argv-owned, alive for the host lifetime */
} sugarbee_engine_t;

static sugarbee_engine_t g_engine = {0};

void trigger_guru_meditation(uint32_t code, const char* title, const char* message, const char* trace);
void render_guru_meditation(void);
static void end_cartridge(void);

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
    if (lua_status == LUA_OK) lua_status = lua_pcall(g_engine.L, 0, 0, 0);
    if (lua_status != LUA_OK) {
        const char* lua_err = lua_tostring(g_engine.L, -1);
        trigger_guru_meditation(0x00000004, "LUA CARTRIDGE ERROR", lua_err, NULL);
        lua_pop(g_engine.L, 1);
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
        "DEVELOPMENT BUILD - GURU RECOVERY CONTROLS NOT YET IMPLEMENTED",
        60, 640, 150, 150, 150, 2);
}

/* A Lua callback may optionally receive the fixed simulation delta. */
void call_lua_global_function(const char* func_name, bool with_dt, double dt) {
    if (!g_engine.L || g_engine.guru_active) return;

    lua_getglobal(g_engine.L, func_name);
    if (lua_isfunction(g_engine.L, -1)) {
        if (with_dt) lua_pushnumber(g_engine.L, dt);
        if (lua_pcall(g_engine.L, with_dt ? 1 : 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(g_engine.L, -1);
            trigger_guru_meditation(0x00000004, "LUA RUNTIME EXCEPTION", err, NULL);
            lua_pop(g_engine.L, 1);
        }
    } else {
        lua_pop(g_engine.L, 1);
    }
}

bool init_engine(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) return false;

    g_engine.graphics = sb_graphics_create("SugarBee Console Host",
                                            SB_DEFAULT_WIDTH, SB_DEFAULT_HEIGHT);
    if (!g_engine.graphics) return false;

    g_engine.input = sb_input_create();
    if (!g_engine.input) return false;

    g_engine.L = luaL_newstate();
    if (!g_engine.L) return false;
    luaL_openlibs(g_engine.L);
    register_sugarbee_api(g_engine.L);

    g_engine.is_running = true;
    return true;
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
                }

                bool pause_changed = false;

                /* The idle host can be closed with B or keyboard K. */
                if (!g_engine.cart_loaded && back_pressed) {
                    g_engine.is_running = false;
                }

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

                        if (g_engine.paused || pause_changed) {
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
                    g_engine.cartridge_exit_requested) {
                    break;
                }
            }
        }

        if (!g_engine.is_running) break;

        if (g_engine.cartridge_exit_requested) {
            end_cartridge();
            sb_timing_reset(&timing);
            last_counter = SDL_GetPerformanceCounter();
        }

        if (g_engine.guru_active) {
            render_guru_meditation();
        } else if (!g_engine.cart_loaded) {
            sb_graphics_clear(g_engine.graphics, 20, 20, 20);
            sb_graphics_text(g_engine.graphics,
                             "SugarBee", 512, 260,
                             255, 220, 80, 4);
            sb_graphics_text(g_engine.graphics,
                             "No cartridge running", 480, 340,
                             255, 255, 255, 2);
            sb_graphics_text(g_engine.graphics,
                             "B / K: Close SugarBee", 472, 390,
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

    if (lua_pcall(g_engine.L, 0, 0, 0) != LUA_OK) {
        const char *err = lua_tostring(g_engine.L, -1);
        fprintf(stderr, "Lua shutdown error: %s\n",
        err ? err : "(non-string error)");
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


int main(int argc, char* argv[]) {
    if (!init_engine()) {
        shutdown_engine();
        return 1;
    }

    if (argc > 1) {
        // Load target cartridge passed via command line
        if (load_virtual_cartridge(argv[1])) {
            call_lua_global_function("init", false, 0.0);
            g_engine.cart_initialized = !g_engine.guru_active;
        }
    } else {
        trigger_guru_meditation(0x00000000, "NO CARTRIDGE INSERTED", "Please specify a .vcart path: ./sugarbee game.vcart", NULL);
    }

    main_loop();
    shutdown_engine();
    return 0;
}