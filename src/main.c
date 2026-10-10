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

// minizip and zlib headers for in-memory / stream unpacking
#include <zlib.h>
#include <unzip.h>

#include "vcart.h"
#include "sb_graphics.h"
#include "sb_input.h"
#include "sb_timing.h"

typedef struct {
    sb_graphics_t* graphics;
    sb_input_t* input;
    lua_State* L;
    bool is_running;
    bool guru_active;
    
    // Diagnostic state
    uint32_t error_code;
    char error_title[64];
    char error_msg[256];
    char stack_trace[512];
    
    // Active Cartridge Data
    vcart_header_t current_cart;
    bool cart_loaded;
} sugarbee_engine_t;

static sugarbee_engine_t g_engine = {0};

void trigger_guru_meditation(uint32_t code, const char* title, const char* message, const char* trace);
void render_guru_meditation(void);

// --- IN-MEMORY ZIP EXTRACTION ---

/* Reads a specific file (e.g. "main.lua") from a .vcart ZIP payload in memory */
char* extract_file_from_vcart_payload(const char* vcart_path, const char* file_to_extract, size_t* out_size) {
    unzFile zip_file = unzOpen(vcart_path);
    if (!zip_file) {
        return NULL;
    }

    // Seek past the 64-byte binary header to start of ZIP payload
    // minizip unzOpen reads from byte 0, so we locate the first file
    if (unzGoToFirstFile(zip_file) != UNZ_OK) {
        unzClose(zip_file);
        return NULL;
    }

    // Iterate through archive files searching for requested target
    bool found = false;
    char current_filename[256];
    unz_file_info file_info;

    do {
        if (unzGetCurrentFileInfo(zip_file, &file_info, current_filename, sizeof(current_filename), NULL, 0, NULL, 0) == UNZ_OK) {
            if (strcmp(current_filename, file_to_extract) == 0) {
                found = true;
                break;
            }
        }
    } while (unzGoToNextFile(zip_file) == UNZ_OK);

    if (!found) {
        unzClose(zip_file);
        return NULL;
    }

    // Open target file inside zip stream
    if (unzOpenCurrentFile(zip_file) != UNZ_OK) {
        unzClose(zip_file);
        return NULL;
    }

    // Allocate heap buffer for extracted payload contents
    size_t uncompressed_size = file_info.uncompressed_size;
    char* buffer = (char*)malloc(uncompressed_size + 1);
    if (!buffer) {
        unzCloseCurrentFile(zip_file);
        unzClose(zip_file);
        return NULL;
    }

    int bytes_read = unzReadCurrentFile(zip_file, buffer, (unsigned int)uncompressed_size);
    if (bytes_read < 0 || (size_t)bytes_read != uncompressed_size) {
        free(buffer);
        unzCloseCurrentFile(zip_file);
        unzClose(zip_file);
        return NULL;
    }

    buffer[uncompressed_size] = '\0'; // Null terminate string buffer
    if (out_size) *out_size = uncompressed_size;

    unzCloseCurrentFile(zip_file);
    unzClose(zip_file);
    return buffer;
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

    // Step 2: Unpack main.lua dynamically from embedded ZIP payload
    size_t script_size = 0;
    char* main_lua_source = extract_file_from_vcart_payload(filepath, "main.lua", &script_size);

    if (!main_lua_source) {
        trigger_guru_meditation(0x00000003, "MISSING ENTRY POINT", "Archive contains no 'main.lua' script at cartridge root.", NULL);
        return false;
    }

    // Step 3: Execute in-memory script string inside embedded Lua VM
    if (luaL_dostring(g_engine.L, main_lua_source) != LUA_OK) {
        const char* lua_err = lua_tostring(g_engine.L, -1);
        trigger_guru_meditation(0x00000004, "LUA COMPILATION ERROR", lua_err, NULL);
        free(main_lua_source);
        return false;
    }

    free(main_lua_source);
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

/* Phase 3A: in-memory RGBA textures only; .vcart asset reads arrive in 3B. */
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

static int l_sugarbee_sprite(lua_State *L) {
    lua_Integer texture = luaL_checkinteger(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    if (texture <= 0 || (uint64_t)texture > UINT32_MAX ||
        !sb_graphics_sprite(g_engine.graphics, (sb_texture_t)texture, x, y))
        return luaL_error(L, "invalid texture handle or sprite drawing failed");
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

static int l_sugarbee_btn(lua_State* L) {
    const char *name = luaL_checkstring(L, 1);
    int player = (int)luaL_optinteger(L, 2, 0);
    lua_pushboolean(L, sb_input_down(g_engine.input, player, button_from_name(name)));
    return 1;
}

/* True for one host update when a button changes from up to down. */
static int l_sugarbee_btnp(lua_State* L) {
    const char *name = luaL_checkstring(L, 1);
    int player = (int)luaL_optinteger(L, 2, 0);
    lua_pushboolean(L, sb_input_pressed(g_engine.input, player, button_from_name(name)));
    return 1;
}

void register_sugarbee_api(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_sugarbee_cls);   lua_setfield(L, -2, "cls");
    lua_pushcfunction(L, l_sugarbee_rect);  lua_setfield(L, -2, "rect");
    lua_pushcfunction(L, l_sugarbee_print); lua_setfield(L, -2, "print");
    lua_pushcfunction(L, l_sugarbee_texture_rgba); lua_setfield(L, -2, "texture_rgba");
    lua_pushcfunction(L, l_sugarbee_sprite); lua_setfield(L, -2, "sprite");
    lua_pushcfunction(L, l_sugarbee_texture_free); lua_setfield(L, -2, "texture_free");
    lua_pushcfunction(L, l_sugarbee_btn);   lua_setfield(L, -2, "btn");
    lua_pushcfunction(L, l_sugarbee_btnp);  lua_setfield(L, -2, "btnp");
    lua_setglobal(L, "sugarbee");
}

// --- TEXT RENDERER & GURU MEDITATION ---

void trigger_guru_meditation(uint32_t code, const char* title, const char* message, const char* trace) {
    g_engine.guru_active = true;
    g_engine.error_code = code;
    strncpy(g_engine.error_title, title, sizeof(g_engine.error_title) - 1);
    strncpy(g_engine.error_msg, message, sizeof(g_engine.error_msg) - 1);
    if (trace) strncpy(g_engine.stack_trace, trace, sizeof(g_engine.stack_trace) - 1);
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
    sb_graphics_text(g_engine.graphics, g_engine.error_msg, 60, 200, 200, 200, 200, 2);

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
                call_lua_global_function("update", true, sb_timing_step_seconds());
                sb_input_end_update(g_engine.input);
                if (g_engine.guru_active) break;
            }
        }

        if (g_engine.guru_active) {
            render_guru_meditation();
        } else {
            sb_graphics_clear(g_engine.graphics, 0, 0, 0);
            call_lua_global_function("draw", false, 0.0);
            if (g_engine.guru_active) render_guru_meditation();
        }

        sb_graphics_present(g_engine.graphics);
        /* Software-renderer fallback may not have VSync; don't spin idle. */
        if (!g_engine.guru_active && updates == 0) SDL_Delay(1);
    }
}

void shutdown_engine(void) {
    if (g_engine.L) lua_close(g_engine.L);
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
        }
    } else {
        trigger_guru_meditation(0x00000000, "NO CARTRIDGE INSERTED", "Please specify a .vcart path: ./sugarbee game.vcart", NULL);
    }

    main_loop();
    shutdown_engine();
    return 0;
}