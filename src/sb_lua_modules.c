#include "sb_lua_modules.h"
#include "sb_assets.h"
#include <lauxlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* safe_path allows at most 239 bytes; reserve four for ".lua". */
#define MODULE_NAME_MAX 235
typedef struct {
    size_t source_bytes;
    unsigned cached, depth;
    char loading[SB_MODULE_MAX_DEPTH][MODULE_NAME_MAX + 1];
} module_state;

/* Protected compiler: borrowed native bytes/length/chunk name -> function or error.
 * Never exposed to Lua. Caller frees bytes even if compilation allocates/fails. */
static int compile_source(lua_State *L) {
    const char *bytes = lua_touserdata(L, 1);
    size_t length = (size_t)lua_tointeger(L, 2);
    const char *chunk = lua_tostring(L, 3);
    if (luaL_loadbufferx(L, bytes, length, chunk, "t") != LUA_OK)
        return lua_error(L);
    return 1;
}

/* Protected worker arguments: state, cache, cartridge, name, member.
 * Returns one cached value; errors leave cleanup to require_module. */
static int load_module(lua_State *L) {
    module_state *state = lua_touserdata(L, 1);
    const char *cartridge = lua_tostring(L, 3);
    const char *member = lua_tostring(L, 5);
    size_t remaining = SB_MODULE_MAX_TOTAL - state->source_bytes;
    size_t cap = remaining < SB_MODULE_MAX_SOURCE ? remaining : SB_MODULE_MAX_SOURCE;
    char chunk[MODULE_NAME_MAX + 6];
    snprintf(chunk, sizeof chunk, "@%s", member);
    luaL_checkstack(L, 4, "compiling module");
    lua_pushcfunction(L, compile_source);
    lua_pushstring(L, chunk); /* Allocate before acquiring the native buffer. */
    uint8_t *bytes = NULL;
    size_t length = 0, charged = 0;
    sb_asset_result_t result = sb_asset_read_limited_counted(
        cartridge, member, cap, &bytes, &length, &charged);
    state->source_bytes += charged;
    if (result != SB_ASSET_OK) {
        if (result == SB_ASSET_TOO_LARGE)
            return luaL_error(L,
                "source/archive limit exceeded (module cap 262144 bytes; session remaining %d bytes)",
                (int)remaining);
        return luaL_error(L, "%s", sb_asset_error_string(result));
    }
    /* [.. helper, chunk] -> [.. helper, bytes, length, chunk] without allocation. */
    lua_pushlightuserdata(L, bytes);
    lua_pushinteger(L, (lua_Integer)length);
    lua_pushvalue(L, -3);
    lua_remove(L, -4);
    int status = lua_pcall(L, 3, 1, 0);
    free(bytes);
    if (status != LUA_OK) return lua_error(L);
    /* Do not install/reset hooks: nested execution shares the host budget. */
    lua_call(L, 0, 1);
    if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushboolean(L, 1); }
    lua_pushvalue(L, 4);
    lua_pushvalue(L, -2);
    lua_rawset(L, 2);
    ++state->cached;
    return 1;
}

/* Upvalues: private state userdata, result cache, copied cartridge path.
 * Stack: exactly one name -> one result (including cached false). */
static int require_module(lua_State *L) {
    if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "require: expected one module-name string");
    size_t length;
    const char *name = lua_tolstring(L, 1, &length);
    if (!length || length > MODULE_NAME_MAX)
        return luaL_error(L, "require: module name must contain 1..235 bytes");
    char member[MODULE_NAME_MAX + 5];
    int start = 1;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c == '.' && !start && i+1 < length) {
            member[i] = '/'; start = 1; continue;
        }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              c == '_' || (!start && c >= '0' && c <= '9')))
            return luaL_error(L, "require: invalid module name (use dotted identifiers)");
        member[i] = (char)c; start = 0;
    }
    memcpy(member + length, ".lua", 5);
    lua_pushvalue(L, 1);
    lua_rawget(L, lua_upvalueindex(2));
    if (!lua_isnil(L, -1)) return 1;
    lua_pop(L, 1);
    module_state *state = lua_touserdata(L, lua_upvalueindex(1));
    for (unsigned i = 0; i < state->depth; ++i) {
        if (strcmp(name, state->loading[i]) == 0) {
            char chain[(MODULE_NAME_MAX + 4) * (SB_MODULE_MAX_DEPTH + 1) + 1] = "";
            for (unsigned j = i; j < state->depth; ++j) {
                strcat(chain, state->loading[j]); strcat(chain, " -> ");
            }
            strcat(chain, name);
            return luaL_error(L, "require: circular import: %s", chain);
        }
    }
    if (state->depth >= SB_MODULE_MAX_DEPTH)
        return luaL_error(L, "require '%s': nested import limit is 16", name);
    /* Reserve cache slots for in-flight parents, which may finish after children. */
    if (state->cached + state->depth >= SB_MODULE_MAX_COUNT)
        return luaL_error(L, "require '%s': module cache limit is 128 (including pending loads)", name);
    luaL_checkstack(L, 6, "loading module");
    lua_pushcfunction(L, load_module);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushvalue(L, lua_upvalueindex(2));
    lua_pushvalue(L, lua_upvalueindex(3));
    lua_pushvalue(L, 1);
    lua_pushstring(L, member);
    strcpy(state->loading[state->depth++], name);
    int status = lua_pcall(L, 5, 1, 0);
    --state->depth; /* Always unwind, including allocation errors and caught limits. */
    if (status != LUA_OK) {
        if (lua_type(L, -1) == LUA_TSTRING && status != LUA_ERRMEM)
            return luaL_error(L, "require '%s': %s", name, lua_tostring(L, -1));
        return lua_error(L); /* Preserve non-string Lua error objects. */
    }
    return 1;
}

void sb_lua_modules_register(lua_State *L, const char *cartridge) {
    module_state *state = lua_newuserdatauv(L, sizeof *state, 0);
    memset(state, 0, sizeof *state);
    lua_newtable(L);
    lua_pushstring(L, cartridge);
    lua_pushcclosure(L, require_module, 3);
    lua_setglobal(L, "require");
}
