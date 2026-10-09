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

// minizip and zlib headers for in-memory / stream unpacking
#include <zlib.h>
#include <unzip.h>

#include "vcart.h"

extern const unsigned char font8x8_basic[128][8];

#define INTERNAL_WIDTH   1280
#define INTERNAL_HEIGHT  720
#define MAX_CONTROLLERS  4

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_GameController* controllers[MAX_CONTROLLERS];
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
void draw_text(const char* text, int x, int y, uint8_t r, uint8_t g, uint8_t b, int scale);

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
    SDL_SetRenderDrawColor(g_engine.renderer, r, g, b, 255);
    SDL_RenderClear(g_engine.renderer);
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

    SDL_Rect rect = { x, y, w, h };
    SDL_SetRenderDrawColor(g_engine.renderer, r, g, b, 255);
    if (filled) SDL_RenderFillRect(g_engine.renderer, &rect);
    else SDL_RenderDrawRect(g_engine.renderer, &rect);
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

    draw_text(text, x, y, r, g, b, scale);
    return 0;
}

static int l_sugarbee_btn(lua_State* L) {
    const char* btn_name = luaL_checkstring(L, 1);
    int player_idx = (int)luaL_optinteger(L, 2, 0);

    if (player_idx < 0 || player_idx >= MAX_CONTROLLERS || !g_engine.controllers[player_idx]) {
        lua_pushboolean(L, false);
        return 1;
    }

    SDL_GameController* pad = g_engine.controllers[player_idx];
    SDL_GameControllerButton button = SDL_CONTROLLER_BUTTON_INVALID;

    if (strcmp(btn_name, "a") == 0) button = SDL_CONTROLLER_BUTTON_A;
    else if (strcmp(btn_name, "b") == 0) button = SDL_CONTROLLER_BUTTON_B;
    else if (strcmp(btn_name, "x") == 0) button = SDL_CONTROLLER_BUTTON_X;
    else if (strcmp(btn_name, "y") == 0) button = SDL_CONTROLLER_BUTTON_Y;
    else if (strcmp(btn_name, "up") == 0) button = SDL_CONTROLLER_BUTTON_DPAD_UP;
    else if (strcmp(btn_name, "down") == 0) button = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    else if (strcmp(btn_name, "left") == 0) button = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    else if (strcmp(btn_name, "right") == 0) button = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    else if (strcmp(btn_name, "start") == 0) button = SDL_CONTROLLER_BUTTON_START;
    else if (strcmp(btn_name, "select") == 0) button = SDL_CONTROLLER_BUTTON_BACK;

    bool pressed = (button != SDL_CONTROLLER_BUTTON_INVALID) && SDL_GameControllerGetButton(pad, button);
    lua_pushboolean(L, pressed);
    return 1;
}

void register_sugarbee_api(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_sugarbee_cls);   lua_setfield(L, -2, "cls");
    lua_pushcfunction(L, l_sugarbee_rect);  lua_setfield(L, -2, "rect");
    lua_pushcfunction(L, l_sugarbee_print); lua_setfield(L, -2, "print");
    lua_pushcfunction(L, l_sugarbee_btn);   lua_setfield(L, -2, "btn");
    lua_setglobal(L, "sugarbee");
}

// --- TEXT RENDERER & GURU MEDITATION ---

void draw_char(char c, int x, int y, uint8_t r, uint8_t g, uint8_t b, int scale) {
    if (c < 32 || c > 126) c = '?';
    const unsigned char* glyph = font8x8_basic[(int)c];

    SDL_SetRenderDrawColor(g_engine.renderer, r, g, b, 255);
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (glyph[row] & (1 << col)) {
                SDL_Rect pixel = { x + (col * scale), y + (row * scale), scale, scale };
                SDL_RenderFillRect(g_engine.renderer, &pixel);
            }
        }
    }
}

void draw_text(const char* text, int x, int y, uint8_t r, uint8_t g, uint8_t b, int scale) {
    int start_x = x;
    while (*text) {
        if (*text == '\n') {
            y += 8 * scale + (2 * scale);
            x = start_x;
        } else {
            draw_char(*text, x, y, r, g, b, scale);
            x += 8 * scale;
        }
        text++;
    }
}

void trigger_guru_meditation(uint32_t code, const char* title, const char* message, const char* trace) {
    g_engine.guru_active = true;
    g_engine.error_code = code;
    strncpy(g_engine.error_title, title, sizeof(g_engine.error_title) - 1);
    strncpy(g_engine.error_msg, message, sizeof(g_engine.error_msg) - 1);
    if (trace) strncpy(g_engine.stack_trace, trace, sizeof(g_engine.stack_trace) - 1);
}

void render_guru_meditation(void) {
    SDL_SetRenderDrawColor(g_engine.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_engine.renderer);

    static uint32_t frame = 0;
    bool flash = ((frame++ / 30) % 2 == 0);

    SDL_SetRenderDrawColor(g_engine.renderer, flash ? 255 : 128, 0, 0, 255);
    SDL_Rect border = { 20, 20, INTERNAL_WIDTH - 40, INTERNAL_HEIGHT - 40 };
    for (int i = 0; i < 6; i++) {
        SDL_RenderDrawRect(g_engine.renderer, &border);
        border.x++; border.y++; border.w -= 2; border.h -= 2;
    }

    char code_str[64];
    snprintf(code_str, sizeof(code_str), "SOFTWARE FAILURE #%08X", g_engine.error_code);

    draw_text("GURU MEDITATION V2", 60, 60, 255, 0, 0, 4);
    draw_text(code_str, 60, 110, 255, 0, 0, 3);
    draw_text(g_engine.error_title, 60, 160, 255, 255, 255, 2);
    draw_text(g_engine.error_msg, 60, 200, 200, 200, 200, 2);

    draw_text("PRESS [START] TO SAVE LOG TO USB | PRESS [A] TO SOFT RESET", 60, 640, 150, 150, 150, 2);
}

void call_lua_global_function(const char* func_name) {
    if (!g_engine.L || g_engine.guru_active) return;

    lua_getglobal(g_engine.L, func_name);
    if (lua_isfunction(g_engine.L, -1)) {
        if (lua_pcall(g_engine.L, 0, 0, 0) != LUA_OK) {
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

    g_engine.window = SDL_CreateWindow("SugarBee Console Host",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       INTERNAL_WIDTH, INTERNAL_HEIGHT, SDL_WINDOW_SHOWN);
    if (!g_engine.window) return false;

    g_engine.renderer = SDL_CreateRenderer(g_engine.window, -1, 
                                           SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_engine.renderer) return false;

    SDL_RenderSetLogicalSize(g_engine.renderer, INTERNAL_WIDTH, INTERNAL_HEIGHT);

    g_engine.L = luaL_newstate();
    if (!g_engine.L) return false;
    luaL_openlibs(g_engine.L);
    register_sugarbee_api(g_engine.L);

    g_engine.is_running = true;
    return true;
}

void main_loop(void) {
    SDL_Event event;

    while (g_engine.is_running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) g_engine.is_running = false;
        }

        if (g_engine.guru_active) {
            render_guru_meditation();
        } else {
            call_lua_global_function("update");

            SDL_SetRenderDrawColor(g_engine.renderer, 0, 0, 0, 255);
            SDL_RenderClear(g_engine.renderer);

            call_lua_global_function("draw");
        }

        SDL_RenderPresent(g_engine.renderer);
    }
}

void shutdown_engine(void) {
    if (g_engine.L) lua_close(g_engine.L);
    if (g_engine.renderer) SDL_DestroyRenderer(g_engine.renderer);
    if (g_engine.window) SDL_DestroyWindow(g_engine.window);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    if (!init_engine()) return 1;

    if (argc > 1) {
        // Load target cartridge passed via command line
        if (load_virtual_cartridge(argv[1])) {
            call_lua_global_function("init");
        }
    } else {
        trigger_guru_meditation(0x00000000, "NO CARTRIDGE INSERTED", "Please specify a .vcart path: ./sugarbee game.vcart", NULL);
    }

    main_loop();
    shutdown_engine();
    return 0;
}