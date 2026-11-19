#include "sb_lua_data.h"
#include "sb_assets.h"
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

typedef struct { size_t bytes_read; } data_state;

/* Protected conversion: borrowed native bytes + length -> one Lua string.
 * Caller always frees the native buffer, including on Lua allocation failure. */
static int copy_bytes(lua_State *L) {
    const char *bytes = lua_touserdata(L, 1);
    size_t length = (size_t)lua_tointeger(L, 2);
    lua_pushlstring(L, bytes, length);
    return 1;
}

static const char *read_error(sb_asset_result_t result) {
    switch (result) {
        case SB_ASSET_NOT_FOUND: return "not_found";
        case SB_ASSET_TOO_LARGE: return "too_large";
        case SB_ASSET_INVALID_CARTRIDGE: return "invalid_cartridge";
        case SB_ASSET_CORRUPT: return "corrupt";
        default: return "io_error";
    }
}

/* Upvalues: private counter userdata, copied cartridge path.
 * Stack: exactly one path string -> (bytes, nil) or (nil, error_code).
 * Invalid API arguments raise; no budget refund on CRC or allocation failure. */
static int read_file(lua_State *L) {
    if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "read_file: expected one path string");
    size_t length;
    const char *path = lua_tolstring(L, 1, &length);
    if (length > 239 || memchr(path, 0, length) || !sb_asset_path_valid(path))
        return luaL_error(L, "read_file: invalid cartridge-relative path");
    data_state *state = lua_touserdata(L, lua_upvalueindex(1));
    const char *cartridge = lua_tostring(L, lua_upvalueindex(2));
    size_t remaining = SB_DATA_MAX_TOTAL - state->bytes_read;
    size_t cap = remaining < SB_DATA_MAX_FILE ? remaining : SB_DATA_MAX_FILE;

    /* Prepare the protected call before acquiring any native allocation. */
    luaL_checkstack(L, 4, "reading cartridge data");
    lua_pushcfunction(L, copy_bytes);
    uint8_t *bytes = NULL;
    size_t size = 0, charged = 0;
    sb_asset_result_t result = sb_asset_read_limited_counted(
        cartridge, path, cap, &bytes, &size, &charged);
    state->bytes_read += charged;
    if (result != SB_ASSET_OK) {
        free(bytes);
        lua_pop(L, 1); /* prepared helper */
        lua_pushnil(L);
        lua_pushstring(L, read_error(result));
        return 2;
    }
    /* [path, helper] -> [path, helper, bytes, length]; no allocation needed. */
    lua_pushlightuserdata(L, bytes);
    lua_pushinteger(L, (lua_Integer)size);
    int status = lua_pcall(L, 2, 1, 0);
    free(bytes);
    if (status != LUA_OK) return lua_error(L);
    lua_pushnil(L);
    return 2;
}

void sb_lua_data_register(lua_State *L, const char *cartridge) {
    data_state *state = lua_newuserdatauv(L, sizeof *state, 0);
    state->bytes_read = 0;
    lua_pushstring(L, cartridge);
    lua_pushcclosure(L, read_file, 2);
    lua_setfield(L, -2, "read_file");
}
