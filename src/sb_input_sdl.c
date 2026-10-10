/* SDL2 backend for SugarBee's language-independent input service. */
#include "sb_input.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_GameController *handle;
    SDL_JoystickID instance_id;
    bool down[SB_BUTTON_COUNT];
} sb_pad_t;

struct sb_input {
    sb_pad_t pads[SB_MAX_PLAYERS];
    bool keyboard[SDL_NUM_SCANCODES];
    bool down[SB_MAX_PLAYERS][SB_BUTTON_COUNT];
    bool pressed[SB_MAX_PLAYERS][SB_BUTTON_COUNT];
    bool quit_requested;
};

static sb_button_t map_controller_button(Uint8 button) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    return SB_BUTTON_UP;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return SB_BUTTON_DOWN;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return SB_BUTTON_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return SB_BUTTON_RIGHT;
        case SDL_CONTROLLER_BUTTON_A:          return SB_BUTTON_A;
        case SDL_CONTROLLER_BUTTON_B:          return SB_BUTTON_B;
        case SDL_CONTROLLER_BUTTON_X:          return SB_BUTTON_X;
        case SDL_CONTROLLER_BUTTON_Y:          return SB_BUTTON_Y;
        case SDL_CONTROLLER_BUTTON_START:      return SB_BUTTON_START;
        case SDL_CONTROLLER_BUTTON_BACK:       return SB_BUTTON_SELECT;
        default:                               return SB_BUTTON_INVALID;
    }
}

/* Two common keyboard layouts for directions; face buttons use the right hand. */
static bool keyboard_down(const sb_input_t *input, sb_button_t button) {
    const bool *key = input->keyboard;
    switch (button) {
        case SB_BUTTON_UP:     return key[SDL_SCANCODE_UP] || key[SDL_SCANCODE_W];
        case SB_BUTTON_DOWN:   return key[SDL_SCANCODE_DOWN] || key[SDL_SCANCODE_S];
        case SB_BUTTON_LEFT:   return key[SDL_SCANCODE_LEFT] || key[SDL_SCANCODE_A];
        case SB_BUTTON_RIGHT:  return key[SDL_SCANCODE_RIGHT] || key[SDL_SCANCODE_D];
        case SB_BUTTON_A:      return key[SDL_SCANCODE_J];
        case SB_BUTTON_B:      return key[SDL_SCANCODE_K];
        case SB_BUTTON_X:      return key[SDL_SCANCODE_U];
        case SB_BUTTON_Y:      return key[SDL_SCANCODE_I];
        case SB_BUTTON_START:  return key[SDL_SCANCODE_RETURN];
        case SB_BUTTON_SELECT: return key[SDL_SCANCODE_RSHIFT];
        default:               return false;
    }
}

static void close_pad(sb_pad_t *pad) {
    if (pad->handle) SDL_GameControllerClose(pad->handle);
    memset(pad, 0, sizeof(*pad));
}

static void open_device(sb_input_t *input, int device_index) {
    if (device_index < 0 || !SDL_IsGameController(device_index)) return;

    /* Ignore duplicate DEVICEADDED events for an already-open controller. */
    SDL_JoystickID instance = SDL_JoystickGetDeviceInstanceID(device_index);
    if (instance < 0) return;
    for (int i = 0; i < SB_MAX_PLAYERS; ++i) {
        if (input->pads[i].handle && input->pads[i].instance_id == instance) return;
    }

    for (int i = 0; i < SB_MAX_PLAYERS; ++i) {
        sb_pad_t *pad = &input->pads[i];
        if (pad->handle) continue;
        pad->handle = SDL_GameControllerOpen(device_index);
        if (pad->handle) {
            pad->instance_id = SDL_JoystickInstanceID(
                SDL_GameControllerGetJoystick(pad->handle));
        }
        return;
    }
}

static sb_pad_t *find_pad(sb_input_t *input, SDL_JoystickID instance) {
    for (int i = 0; i < SB_MAX_PLAYERS; ++i) {
        if (input->pads[i].handle && input->pads[i].instance_id == instance)
            return &input->pads[i];
    }
    return NULL;
}

sb_input_t *sb_input_create(void) {
    if (!(SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER))
        return NULL;
    sb_input_t *input = calloc(1, sizeof(*input));
    if (!input) return NULL;

    SDL_GameControllerEventState(SDL_ENABLE);
    for (int i = 0, n = SDL_NumJoysticks(); i < n; ++i) {
        open_device(input, i);
    }
    return input;
}

void sb_input_destroy(sb_input_t *input) {
    if (!input) return;
    for (int i = 0; i < SB_MAX_PLAYERS; ++i) close_pad(&input->pads[i]);
    free(input);
}

bool sb_input_poll(sb_input_t *input) {
    if (!input || input->quit_requested) return false;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                input->quit_requested = true;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                    input->quit_requested = true;
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    memset(input->keyboard, 0, sizeof(input->keyboard));
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                SDL_Scancode key = event.key.keysym.scancode;
                if (key >= 0 && key < SDL_NUM_SCANCODES)
                    input->keyboard[key] = event.type == SDL_KEYDOWN;
                break;
            }
            case SDL_CONTROLLERDEVICEADDED:
                open_device(input, event.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED: {
                sb_pad_t *pad = find_pad(input, event.cdevice.which);
                if (pad) close_pad(pad);
                break;
            }
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                sb_pad_t *pad = find_pad(input, event.cbutton.which);
                sb_button_t button = map_controller_button(event.cbutton.button);
                if (pad && button != SB_BUTTON_INVALID)
                    pad->down[button] = event.type == SDL_CONTROLLERBUTTONDOWN;
                break;
            }
            default:
                break;
        }
    }

    for (int player = 0; player < SB_MAX_PLAYERS; ++player) {
        for (int button = 0; button < SB_BUTTON_COUNT; ++button) {
            bool next = input->pads[player].down[button];
            /* A keyboard is an optional second input device for player zero. */
            if (player == 0) next |= keyboard_down(input, (sb_button_t)button);
            input->pressed[player][button] = next && !input->down[player][button];
            input->down[player][button] = next;
        }
    }
    return !input->quit_requested;
}

static bool valid_query(const sb_input_t *input, int player, sb_button_t button) {
    return input && player >= 0 && player < SB_MAX_PLAYERS &&
           button >= SB_BUTTON_UP && button < SB_BUTTON_COUNT;
}

bool sb_input_down(const sb_input_t *input, int player, sb_button_t button) {
    return valid_query(input, player, button) && input->down[player][button];
}

bool sb_input_pressed(const sb_input_t *input, int player, sb_button_t button) {
    return valid_query(input, player, button) && input->pressed[player][button];
}

int sb_input_controller_count(const sb_input_t *input) {
    int count = 0;
    if (!input) return 0;
    for (int i = 0; i < SB_MAX_PLAYERS; ++i) {
        if (input->pads[i].handle) ++count;
    }
    return count;
}
