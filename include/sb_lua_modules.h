#ifndef SB_LUA_MODULES_H
#define SB_LUA_MODULES_H
#include <lua.h>
#define SB_MODULE_MAX_SOURCE (256u * 1024u)
#define SB_MODULE_MAX_TOTAL (8u * 1024u * 1024u)
#define SB_MODULE_MAX_COUNT 128u
#define SB_MODULE_MAX_DEPTH 16u
/* Installs cartridge-only require. Stack unchanged. Copies cartridge path;
 * private cache/counters belong to this Lua state and die with it.
 * Call once per fresh cartridge state, before executing main.lua.
 * Lua allocation errors propagate; no native resources are retained. */
void sb_lua_modules_register(lua_State *L, const char *cartridge);
#endif
