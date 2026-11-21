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
    bool pending_pressed[SB_MAX_PLAYERS][SB_BUTTON_COUNT];
    bool quit_requested;
    bool blocked_keys[SDL_NUM_SCANCODES];
    bool keyboard_pending[SB_BUTTON_COUNT];
    bool keyboard_pressed[SB_BUTTON_COUNT];
    bool text_mode, text_suspended, unfocused, in_update;
    sb_text_batch_t text_pending, text_current;

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
#define KEY(k) (input->keyboard[k] && !input->blocked_keys[k] && !input->unfocused)
    bool typing = sb_input_text_active(input);
    if (typing && button != SB_BUTTON_START) return false;
    switch (button) {
        case SB_BUTTON_UP:     return KEY(SDL_SCANCODE_UP) || KEY(SDL_SCANCODE_W);
        case SB_BUTTON_DOWN:   return KEY(SDL_SCANCODE_DOWN) || KEY(SDL_SCANCODE_S);
        case SB_BUTTON_LEFT:   return KEY(SDL_SCANCODE_LEFT) || KEY(SDL_SCANCODE_A);
        case SB_BUTTON_RIGHT:  return KEY(SDL_SCANCODE_RIGHT) || KEY(SDL_SCANCODE_D);
        case SB_BUTTON_A:      return KEY(SDL_SCANCODE_J);
        case SB_BUTTON_B:      return KEY(SDL_SCANCODE_K);
        case SB_BUTTON_X:      return KEY(SDL_SCANCODE_U);
        case SB_BUTTON_Y:      return KEY(SDL_SCANCODE_I);
        case SB_BUTTON_START:  return KEY(SDL_SCANCODE_ESCAPE) ||
            (!typing && (KEY(SDL_SCANCODE_RETURN) || KEY(SDL_SCANCODE_KP_ENTER)));
        case SB_BUTTON_SELECT: return KEY(SDL_SCANCODE_RSHIFT);
        default:               return false;
    }
}

#undef KEY

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

/* SDL extended composition events own an allocation, unlike fixed text events. */
static void flush_text_events(void) {
    SDL_FlushEvent(SDL_TEXTINPUT);
    SDL_FlushEvent(SDL_TEXTEDITING);
#if SDL_VERSION_ATLEAST(2, 0, 22)
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_TEXTEDITING_EXT, SDL_TEXTEDITING_EXT) == 1)
        SDL_free(event.editExt.text);
#endif
}

sb_input_t *sb_input_create(void) {
    if (!(SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER))
        return NULL;
    sb_input_t *input = calloc(1, sizeof(*input));
    if (!input) return NULL;

    SDL_StopTextInput();
    SDL_GameControllerEventState(SDL_ENABLE);
    for (int i = 0, n = SDL_NumJoysticks(); i < n; ++i) {
        open_device(input, i);
    }
    return input;
}

void sb_input_destroy(sb_input_t *input) {
    if (!input) return;
    SDL_StopTextInput();
    flush_text_events();
    for (int i = 0; i < SB_MAX_PLAYERS; ++i) close_pad(&input->pads[i]);
    free(input);
}

/* Update the held snapshot and latch rising edges. Sampling after each
 * event also catches quick down/up pairs drained in the same host poll. */
static void sample_buttons(sb_input_t *input) {
    for (int player = 0; player < SB_MAX_PLAYERS; ++player) {
        for (int button = 0; button < SB_BUTTON_COUNT; ++button) {
            bool pad = input->pads[player].down[button];
            bool key = player == 0 && keyboard_down(input, (sb_button_t)button);
            bool next = pad || key;
            if (next && !input->down[player][button]) {
                if (pad) input->pending_pressed[player][button] = true;
                if (key) input->keyboard_pending[button] = true;
            }
            input->down[player][button] = next;
        }
    }
}

void sb_input_begin_update(sb_input_t *input) {
    if (!input) return;
    memcpy(input->pressed, input->pending_pressed, sizeof(input->pressed));
    memset(input->pending_pressed, 0, sizeof(input->pending_pressed));
    memcpy(input->keyboard_pressed, input->keyboard_pending, sizeof(input->keyboard_pressed));
    memset(input->keyboard_pending, 0, sizeof(input->keyboard_pending));
    input->text_current = input->text_pending;
    memset(&input->text_pending, 0, sizeof(input->text_pending));
    input->in_update = true;
}

void sb_input_end_update(sb_input_t *input) {
    if (!input) return;
    memset(input->pressed, 0, sizeof(input->pressed));
    memset(input->keyboard_pressed, 0, sizeof(input->keyboard_pressed));
    memset(&input->text_current, 0, sizeof(input->text_current));
    input->in_update = false;
}

bool sb_input_text_active(const sb_input_t *input) {
    return input && input->text_mode && !input->text_suspended && !input->unfocused;
}

/* Preserve physical state, but require release before it can act in a new mode.
 * Only keyboard edges are cleared: physical controller edges remain available. */
static void text_transition(sb_input_t *input) {
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i)
        input->blocked_keys[i] = input->keyboard[i];
    memset(input->keyboard_pending, 0, sizeof(input->keyboard_pending));
    memset(input->keyboard_pressed, 0, sizeof(input->keyboard_pressed));
    memset(&input->text_pending, 0, sizeof(input->text_pending));
    memset(&input->text_current, 0, sizeof(input->text_current));
    flush_text_events();
    if (sb_input_text_active(input)) SDL_StartTextInput();
    else SDL_StopTextInput();
    sample_buttons(input);
}
void sb_input_text_mode(sb_input_t *input, bool enabled) {
    if (!input || input->text_mode == enabled) return;
    input->text_mode = enabled;
    text_transition(input);
}
void sb_input_text_suspend(sb_input_t *input, bool suspended) {
    if (!input || input->text_suspended == suspended) return;
    input->text_suspended = suspended;
    text_transition(input);
}
void sb_input_text_reset(sb_input_t *input) {
    if (!input) return;
    input->text_mode = false;
    input->text_suspended = false;
    text_transition(input);
}
void sb_input_text_read(sb_input_t *input, sb_text_batch_t *batch) {
    if (!batch) return;
    memset(batch, 0, sizeof(*batch));
    if (!sb_input_text_active(input) || !input->in_update) return;
    *batch = input->text_current;
    memset(&input->text_current, 0, sizeof(input->text_current));
}
static void text_reject(sb_input_t *input, sb_text_status_t status) {
    if (input->text_pending.status == SB_TEXT_OK) {
        input->text_pending.count = 0;
        input->text_pending.status = status;
    }
}
static void text_append(sb_input_t *input, sb_text_kind_t kind, char character) {
    sb_text_batch_t *batch = &input->text_pending;
    if (batch->status != SB_TEXT_OK) return;
    if (batch->count == SB_TEXT_EVENT_CAPACITY) {
        text_reject(input, SB_TEXT_OVERFLOW);
        return;
    }
    batch->events[batch->count++] = (sb_text_event_t){kind, character};
}
static bool blocked_text_key(const sb_input_t *input) {
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i)
        if (input->keyboard[i] && input->blocked_keys[i]) return true;
    return false;
}
static void text_key(sb_input_t *input, const SDL_KeyboardEvent *event) {
    sb_text_kind_t kind;
    switch (event->keysym.scancode) {
        case SDL_SCANCODE_BACKSPACE: kind = SB_TEXT_BACKSPACE; break;
        case SDL_SCANCODE_DELETE: kind = SB_TEXT_DELETE; break;
        case SDL_SCANCODE_LEFT: kind = SB_TEXT_LEFT; break;
        case SDL_SCANCODE_RIGHT: kind = SB_TEXT_RIGHT; break;
        case SDL_SCANCODE_HOME: kind = SB_TEXT_HOME; break;
        case SDL_SCANCODE_END: kind = SB_TEXT_END; break;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            if (event->repeat) return;
            kind = SB_TEXT_SUBMIT; break;
        default: return;
    }
    text_append(input, kind, 0);
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
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    input->unfocused = true;
                    memset(input->keyboard, 0, sizeof(input->keyboard));
                    text_transition(input);
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    input->unfocused = false;
                    text_transition(input);
                }
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                SDL_Scancode key = event.key.keysym.scancode;
                if (key >= 0 && key < SDL_NUM_SCANCODES) {
                    bool down = event.type == SDL_KEYDOWN;
                    /* A repeat without a fresh press (e.g. focus return) cannot
                     * activate controls or type until released. */
                    if (down && event.key.repeat && !input->keyboard[key])
                        input->blocked_keys[key] = true;
                    input->keyboard[key] = down;
                    if (!down) input->blocked_keys[key] = false;
                    if (down && sb_input_text_active(input) && !blocked_text_key(input))
                        text_key(input, &event.key);
                }
                break;
            }
            case SDL_TEXTINPUT:
                if (sb_input_text_active(input) && !blocked_text_key(input)) {
                    const unsigned char *text = (const unsigned char *)event.text.text;
                    size_t length = 0;
                    while (length < sizeof(event.text.text) && text[length]) ++length;
                    if (length == sizeof(event.text.text)) text_reject(input, SB_TEXT_UNSUPPORTED);
                    for (size_t i = 0; i < length; ++i) {
                        if (text[i] < 32 || text[i] > 126) {
                            text_reject(input, SB_TEXT_UNSUPPORTED);
                            break;
                        }
                        text_append(input, SB_TEXT_CHARACTER, (char)text[i]);
                    }
                }
                break;
            case SDL_TEXTEDITING:
                /* Composition is not part of the ASCII contract. */
                if (sb_input_text_active(input) && event.edit.text[0])
                    text_reject(input, SB_TEXT_UNSUPPORTED);
                break;
#if SDL_VERSION_ATLEAST(2, 0, 22)
            case SDL_TEXTEDITING_EXT:
                if (sb_input_text_active(input) && event.editExt.text && event.editExt.text[0])
                    text_reject(input, SB_TEXT_UNSUPPORTED);
                SDL_free(event.editExt.text);
                break;
#endif
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
        sample_buttons(input);
    }
    sample_buttons(input);
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
    return valid_query(input, player, button) && (input->pressed[player][button] ||
        (player == 0 && input->keyboard_pressed[button]));
}

int sb_input_controller_count(const sb_input_t *input) {
    int count = 0;
    if (!input) return 0;
    for (int i = 0; i < SB_MAX_PLAYERS; ++i) {
        if (input->pads[i].handle) ++count;
    }
    return count;
}
