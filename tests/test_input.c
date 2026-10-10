/* Headless input smoke test: logical keyboard mapping and edge semantics. */
#include "sb_input.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "Input test failed at line %d: %s\n", __LINE__, #expr); \
        return EXIT_FAILURE; \
    } \
} while (0)

static int send_key(Uint32 type, SDL_Scancode scancode) {
    SDL_Event event = {0};
    event.type = type;
    event.key.type = type;
    event.key.keysym.scancode = scancode;
    return SDL_PushEvent(&event);
}

static int send_focus_lost(void) {
    SDL_Event event = {0};
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    return SDL_PushEvent(&event);
}

int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0);
    sb_input_t *input = sb_input_create();
    CHECK(input != NULL);
    CHECK(sb_input_controller_count(input) == 0);
    CHECK(!sb_input_down(input, 0, SB_BUTTON_LEFT));
    CHECK(!sb_input_down(input, -1, SB_BUTTON_LEFT));
    CHECK(!sb_input_down(input, SB_MAX_PLAYERS, SB_BUTTON_LEFT));
    CHECK(!sb_input_pressed(input, 0, SB_BUTTON_INVALID));

    CHECK(send_key(SDL_KEYDOWN, SDL_SCANCODE_LEFT) == 1);
    CHECK(sb_input_poll(input));
    CHECK(sb_input_down(input, 0, SB_BUTTON_LEFT));
    CHECK(sb_input_pressed(input, 0, SB_BUTTON_LEFT));
    CHECK(!sb_input_down(input, 1, SB_BUTTON_LEFT));

    CHECK(sb_input_poll(input));
    CHECK(sb_input_down(input, 0, SB_BUTTON_LEFT));
    CHECK(!sb_input_pressed(input, 0, SB_BUTTON_LEFT));

    /* A second key mapped to the same direction does not create a new edge. */
    CHECK(send_key(SDL_KEYDOWN, SDL_SCANCODE_A) == 1);
    CHECK(sb_input_poll(input));
    CHECK(sb_input_down(input, 0, SB_BUTTON_LEFT));
    CHECK(!sb_input_pressed(input, 0, SB_BUTTON_LEFT));
    CHECK(send_key(SDL_KEYUP, SDL_SCANCODE_LEFT) == 1);
    CHECK(sb_input_poll(input));
    CHECK(sb_input_down(input, 0, SB_BUTTON_LEFT));
    CHECK(send_key(SDL_KEYUP, SDL_SCANCODE_A) == 1);
    CHECK(sb_input_poll(input));
    CHECK(!sb_input_down(input, 0, SB_BUTTON_LEFT));

    CHECK(send_key(SDL_KEYDOWN, SDL_SCANCODE_J) == 1);
    CHECK(sb_input_poll(input));
    CHECK(sb_input_down(input, 0, SB_BUTTON_A));
    CHECK(sb_input_pressed(input, 0, SB_BUTTON_A));
    CHECK(send_key(SDL_KEYUP, SDL_SCANCODE_J) == 1);
    CHECK(sb_input_poll(input));
    CHECK(!sb_input_down(input, 0, SB_BUTTON_A));

    CHECK(send_key(SDL_KEYDOWN, SDL_SCANCODE_RIGHT) == 1);
    CHECK(sb_input_poll(input));
    CHECK(send_focus_lost() == 1);
    CHECK(sb_input_poll(input));
    CHECK(!sb_input_down(input, 0, SB_BUTTON_RIGHT));

#if SDL_VERSION_ATLEAST(2, 0, 14)
    /* Virtual gamepads are optional on some SDL builds; exercise if present. */
    int virtual_index = SDL_JoystickAttachVirtual(
        SDL_JOYSTICK_TYPE_GAMECONTROLLER, 0, SDL_CONTROLLER_BUTTON_MAX, 0);
    if (virtual_index >= 0) {
        CHECK(sb_input_poll(input));
        if (SDL_IsGameController(virtual_index)) {
            CHECK(sb_input_controller_count(input) == 1);
            SDL_JoystickID instance = SDL_JoystickGetDeviceInstanceID(virtual_index);
            SDL_Event press = {0};
            press.cbutton.type = SDL_CONTROLLERBUTTONDOWN;
            press.cbutton.which = instance;
            press.cbutton.button = SDL_CONTROLLER_BUTTON_A;
            CHECK(SDL_PushEvent(&press) == 1);
            CHECK(sb_input_poll(input));
            CHECK(sb_input_down(input, 0, SB_BUTTON_A));
            CHECK(sb_input_pressed(input, 0, SB_BUTTON_A));
            CHECK(sb_input_poll(input));
            CHECK(!sb_input_pressed(input, 0, SB_BUTTON_A));

            SDL_Event release = {0};
            release.cbutton.type = SDL_CONTROLLERBUTTONUP;
            release.cbutton.which = instance;
            release.cbutton.button = SDL_CONTROLLER_BUTTON_A;
            CHECK(SDL_PushEvent(&release) == 1);
            CHECK(sb_input_poll(input));
            CHECK(!sb_input_down(input, 0, SB_BUTTON_A));
        }
        CHECK(SDL_JoystickDetachVirtual(virtual_index) == 0);
        CHECK(sb_input_poll(input));
        CHECK(sb_input_controller_count(input) == 0);
    } else {
        fprintf(stderr, "SDL virtual joystick unavailable: skipping hotplug test\n");
    }
#endif

    SDL_Event quit = {0};
    quit.type = SDL_QUIT;
    CHECK(SDL_PushEvent(&quit) == 1);
    CHECK(!sb_input_poll(input));
    CHECK(!sb_input_poll(input));

    sb_input_destroy(input);
    SDL_Quit();
    return EXIT_SUCCESS;
}
