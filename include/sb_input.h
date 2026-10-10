/*
 * SugarBee input services (experimental v0.1).
 * Language-independent interface: SDL and Lua types stay in the backend.
 */
#ifndef SB_INPUT_H
#define SB_INPUT_H

#include <stdbool.h>

#define SB_MAX_PLAYERS 4

typedef struct sb_input sb_input_t;

typedef enum {
    SB_BUTTON_INVALID = -1,
    SB_BUTTON_UP = 0,
    SB_BUTTON_DOWN,
    SB_BUTTON_LEFT,
    SB_BUTTON_RIGHT,
    SB_BUTTON_A,
    SB_BUTTON_B,
    SB_BUTTON_X,
    SB_BUTTON_Y,
    SB_BUTTON_START,
    SB_BUTTON_SELECT,
    SB_BUTTON_COUNT
} sb_button_t;

/*
 * SDL's gamecontroller subsystem must be initialized before creation.
 * The input context owns any opened SDL controller handles.
 */
sb_input_t *sb_input_create(void);
void sb_input_destroy(sb_input_t *input);

/*
 * Poll pending platform events and collect button transitions once per host loop.
 * Returns false if the user requested window/application shutdown.
 * Call once before timing/update/draw; consumes SDL's shared event queue.
 */
bool sb_input_poll(sb_input_t *input);

/*
 * A simulation update calls begin_update before invoking game update(dt) and
 * end_update afterward. A new press is buffered even if zero simulation
 * updates occurred since the last host poll. If a frame runs multiple
 * simulation updates, the press is reported on the first one only.
 * btnp/pressed is intended to be queried within update(dt), not draw().
 */
void sb_input_begin_update(sb_input_t *input);
void sb_input_end_update(sb_input_t *input);

/* Player indices are 0..SB_MAX_PLAYERS-1; invalid queries return false. */
bool sb_input_down(const sb_input_t *input, int player, sb_button_t button);
bool sb_input_pressed(const sb_input_t *input, int player, sb_button_t button);

/* Number of currently opened SDL GameController devices, excluding keyboard. */
int sb_input_controller_count(const sb_input_t *input);

#endif /* SB_INPUT_H */
