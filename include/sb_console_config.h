/* Bounded cartridge canvas declaration; independent of SDL and Lua. */
#ifndef SB_CONSOLE_CONFIG_H
#define SB_CONSOLE_CONFIG_H
#include <stdbool.h>
#include <stddef.h>
#define SB_CONSOLE_CONFIG_MAX 4096u
/* Outputs are set to defaults on failure. Error is always terminated when cap>0. */
bool sb_console_config_parse(const void *data, size_t size, int *width, int *height,
                             char *error, size_t cap);
bool sb_console_config_load(const char *cart, int *width, int *height,
                            char *error, size_t cap);
#endif
