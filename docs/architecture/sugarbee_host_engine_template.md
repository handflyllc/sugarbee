/*
 * PROJECT SUGARBEE - Host Engine Core Template
 * Target: C99 / C11 with SDL2, OpenGL ES 3.0 / OpenGL, and Lua 5.4
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// SDL2 Headers
#include <SDL2/SDL.h>

// Lua 5.4 Headers
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// System Constants
#define SYSTEM_NAME "Project SugarBee"
#define SYSTEM_VERSION_MAJOR 0
#define SYSTEM_VERSION_MINOR 1
#define SYSTEM_VERSION_PATCH 0

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// System States
typedef enum {
    STATE_BOOT,
    STATE_RUNNING_CARTRIDGE,
    STATE_GURU_MEDITATION,
    STATE_SHUTDOWN
} EngineState;

// Engine Context
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    lua_State *L;
    EngineState state;
    bool is_running;

    // Guru Meditation Error Info
    char error_code[32];
    char error_message[512];
    char stack_trace[1024];
    bool dev_mode_overlay;
} SugarBeeEngine;

// Global Engine Reference
static SugarBeeEngine engine;

// Historical bitmap data removed: its provenance was not established.
// Link the documented replacement in src/font8x8.c instead.
// See third_party/font8x8/README.md; this historical draft is not the current host.
extern const unsigned char font8x8_basic[128][8];

// --- RENDER CHARACTER ---
void draw_char(char c, int x, int y, int scale, SDL_Color color) {
    if (c < 32 || c > 126) c = '?'; // Fallback
    int char_idx = (unsigned char)c;

    SDL_SetRenderDrawColor(engine.renderer, color.r, color.g, color.b, color.a);

    for (int row = 0; row < 8; row++) {
        unsigned char byte = font8x8_basic[char_idx][row];
        for (int col = 0; col < 8; col++) {
            // Check bit from left to right
            if (byte & (1 << (7 - col))) {
                SDL_Rect pixel_rect = {
                    x + (col * scale),
                    y + (row * scale),
                    scale, scale
                };
                SDL_RenderFillRect(engine.renderer, &pixel_rect);
            }
        }
    }
}

// --- RENDER STRING ---
void draw_text(const char *text, int x, int y, int scale, SDL_Color color) {
    int start_x = x;
    while (*text) {
        if (*text == '\n') {
            y += (8 * scale) + (2 * scale); // Line padding
            x = start_x;
        } else {
            draw_char(*text, x, y, scale, color);
            x += (8 * scale);
        }
        text++;
    }
}

// --- GURU MEDITATION HANDLER ---
void trigger_guru_meditation(const char *code, const char *msg, const char *trace) {
    engine.state = STATE_GURU_MEDITATION;
    snprintf(engine.error_code, sizeof(engine.error_code), "%s", code ? code : "00000000.00000001");
    snprintf(engine.error_message, sizeof(engine.error_message), "%s", msg ? msg : "Unknown system fault.");
    snprintf(engine.stack_trace, sizeof(engine.stack_trace), "%s", trace ? trace : "No stack trace available.");

    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "GURU MEDITATION TRIPPED: [%s] %s\n", engine.error_code, engine.error_message);
}

// --- LUA ERROR HOOK ---
static int lua_panic_handler(lua_State *L) {
    const char *msg = lua_tostring(L, -1);
    trigger_guru_meditation("GURU_LUA_PANIC", msg, "Fatal memory or panic state in Lua VM.");
    return 0;
}

// --- ENGINE INITIALIZATION ---
bool sugarbee_init(void) {
    memset(&engine, 0, sizeof(SugarBeeEngine));
    engine.state = STATE_BOOT;
    engine.is_running = true;
    engine.dev_mode_overlay = false;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("SDL Initialization Error: %s\n", SDL_GetError());
        return false;
    }

    // Create 1280x720 Window
    engine.window = SDL_CreateWindow(
        SYSTEM_NAME " (Phase A - Desktop)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!engine.window) {
        printf("Failed to create SDL Window: %s\n", SDL_GetError());
        return false;
    }

    // Create Accelerated Renderer
    engine.renderer = SDL_CreateRenderer(engine.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!engine.renderer) {
        printf("Failed to create SDL Renderer: %s\n", SDL_GetError());
        return false;
    }

    // Set Logical Size for Clean Scaling (1280x720 base viewport)
    SDL_RenderSetLogicalSize(engine.renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Initialize Embedded Lua VM
    engine.L = luaL_newstate();
    if (!engine.L) {
        printf("Failed to create Lua state VM.\n");
        return false;
    }

    luaL_openlibs(engine.L);
    lua_atpanic(engine.L, lua_panic_handler);

    printf("[%s v%d.%d.%d Initialized Successfully]\n", SYSTEM_NAME, SYSTEM_VERSION_MAJOR, SYSTEM_VERSION_MINOR, SYSTEM_VERSION_PATCH);
    return true;
}

// --- LUA DUMMY SCRIPT LOAD (TESTING GURU FAULT) ---
void load_test_script(void) {
    const char *test_script =
        "function update()\n"
        "   local player = nil\n"
        "   print(player.health)\n"
        "end\n"
        "update()\n";

    if (luaL_dostring(engine.L, test_script) != LUA_OK) {
        const char *err_msg = lua_tostring(engine.L, -1);
        trigger_guru_meditation("00000004.0000002A", err_msg, "STACK TRACE:\n  line 3 in global function 'update'");
    } else {
        engine.state = STATE_RUNNING_CARTRIDGE;
    }
}

// --- PROCESS EVENTS ---
void sugarbee_process_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            engine.is_running = false;
        }
        else if (event.type == SDL_CONTROLLERDEVICEADDED) {
            SDL_GameController *controller = SDL_GameControllerOpen(event.cdevice.which);
            if (controller) {
                printf("[Input] Gamepad Connected: %s\n", SDL_GameControllerName(controller));
            }
        }
        else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            printf("[Input] Gamepad Disconnected (ID: %d)\n", event.cdevice.which);
        }
        else if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    engine.is_running = false;
                    break;
                case SDLK_F1:
                    if (engine.state == STATE_GURU_MEDITATION) {
                        engine.dev_mode_overlay = !engine.dev_mode_overlay;
                    }
                    break;
                case SDLK_r:
                    if (engine.state == STATE_GURU_MEDITATION) {
                        printf("[System Soft Reboot Initiated]\n");
                        engine.state = STATE_RUNNING_CARTRIDGE;
                    }
                    break;
            }
        }
    }
}

// --- RENDER GURU MEDITATION SCREEN ---
void render_guru_meditation(void) {
    static int flash_counter = 0;
    flash_counter++;
    bool flash = (flash_counter / 30) % 2 == 0;

    // Background: Dark Red/Black
    SDL_SetRenderDrawColor(engine.renderer, 10, 0, 0, 255);
    SDL_RenderClear(engine.renderer);

    // Flashing Outer Border
    SDL_Color border_color = flash ? (SDL_Color){220, 20, 20, 255} : (SDL_Color){80, 0, 0, 255};
    SDL_SetRenderDrawColor(engine.renderer, border_color.r, border_color.g, border_color.b, 255);
    
    SDL_Rect border_rect = { 40, 40, SCREEN_WIDTH - 80, SCREEN_HEIGHT - 80 };
    SDL_RenderDrawRect(engine.renderer, &border_rect);
    SDL_Rect inner_rect = { 44, 44, SCREEN_WIDTH - 88, SCREEN_HEIGHT - 88 };
    SDL_RenderDrawRect(engine.renderer, &inner_rect);

    // Render Text Headers
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color red = {220, 40, 40, 255};
    SDL_Color gray = {160, 160, 160, 255};

    draw_text("SOFTWARE FAILURE", 510, 70, 3, red);
    
    char code_str[64];
    snprintf(code_str, sizeof(code_str), "GURU MEDITATION #%s", engine.error_code);
    draw_text(code_str, 420, 110, 2, red);

    // Human-Readable Error
    draw_text("DESCRIPTION:", 80, 180, 2, white);
    draw_text(engine.error_message, 80, 210, 2, gray);

    // Developer Mode Stack Trace Toggle
    if (engine.dev_mode_overlay) {
        draw_text("DEVELOPER DEBUG OVERLAY (F1):", 80, 300, 2, white);
        draw_text(engine.stack_trace, 80, 330, 2, gray);
    } else {
        draw_text("PRESS F1 / L1+R1+SELECT FOR DEBUG STACK TRACE", 80, 300, 2, gray);
    }

    // System Options / Footer
    draw_text("PRESS 'R' TO SOFT RESET SYSTEM", 80, 600, 2, white);
    draw_text("PRESS ESC TO SHUT DOWN ENGINE", 80, 630, 2, gray);
}

// --- RENDER LOOP ---
void sugarbee_render(void) {
    if (engine.state == STATE_GURU_MEDITATION) {
        render_guru_meditation();
    } else {
        // Running state background
        SDL_SetRenderDrawColor(engine.renderer, 40, 25, 10, 255);
        SDL_RenderClear(engine.renderer);
        
        SDL_Color amber = {255, 180, 40, 255};
        draw_text("SUGARBEE ENGINE RUNNING", 440, 340, 3, amber);
    }

    SDL_RenderPresent(engine.renderer);
}

// --- CLEANUP ---
void sugarbee_shutdown(void) {
    if (engine.L) {
        lua_close(engine.L);
    }
    if (engine.renderer) {
        SDL_DestroyRenderer(engine.renderer);
    }
    if (engine.window) {
        SDL_DestroyWindow(engine.window);
    }
    SDL_Quit();
    printf("[%s Engine Shutdown Complete]\n", SYSTEM_NAME);
}

// --- MAIN ENTRY POINT ---
int main(int argc, char *argv[]) {
    if (!sugarbee_init()) {
        return EXIT_FAILURE;
    }

    // Trigger test script error to present Guru Meditation screen
    load_test_script();

    // Main Engine Loop
    while (engine.is_running) {
        sugarbee_process_events();
        sugarbee_render();
        SDL_Delay(16); // ~60 FPS
    }

    sugarbee_shutdown();
    return EXIT_SUCCESS;
}