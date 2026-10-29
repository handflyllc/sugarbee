/* Use the real host library setup without starting SDL. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main

int main(void) {
    /* Repeat with fresh states, as the launcher does for each cartridge. */
    for (int session = 0; session < 2; ++session) {
        lua_State *L = luaL_newstate();
        if (!L) return 1;
        open_cartridge_libraries(L);
        if (lua_gettop(L) != 0) {
            lua_close(L);
            return 1;
        }
        const char *checks =
            "for _, name in ipairs({'coroutine','debug','io','os','package',"
            "'require','dofile','loadfile'}) do "
            "assert(_G[name] == nil, name .. ' must be unavailable') end "
            "local t = {3, 1, 2}; table.sort(t); "
            "assert(table.concat(t, ',') == '1,2,3'); "
            "assert(string.upper('bee') == 'BEE'); "
            "assert(math.floor(2.5) == 2); assert(utf8.len('bee') == 3); "
            "assert(type(print) == 'function'); "
            "assert(assert(load('return 6 * 7'))() == 42); "
            "local ok = pcall(function() return dofile('unused') end); "
            "assert(not ok)";
        if (luaL_dostring(L, checks) != LUA_OK) {
            fprintf(stderr, "Lua library policy failed: %s\n", lua_tostring(L, -1));
            lua_close(L);
            return 1;
        }
        lua_close(L);
    }
    return 0;
}
