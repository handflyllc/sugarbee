/* Use the real host library setup without starting SDL. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main

static int finalizer_calls;

static int record_finalizer(lua_State *L) {
    (void)L;
    ++finalizer_calls;
    return 0;
}

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
        lua_pushcfunction(L, record_finalizer);
        lua_setglobal(L, "record_finalizer");
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
            "assert(not ok); "
            /* Ordinary shared metatables, mutation, removal, and protection. */
            "local mt = {__index = {answer = 42}}; local a, b = {}, {}; "
            "assert(setmetatable(a, mt) == a); setmetatable(b, mt); "
            "assert(a.answer == 42 and b.answer == 42); "
            "mt.__index.answer = 7; assert(a.answer == 7 and b.answer == 7); "
            "assert(getmetatable(a) == mt); assert(setmetatable(a, nil) == a); "
            "assert(a.answer == nil); "
            "local locked = setmetatable({}, {__metatable = 'locked'}); "
            "assert(getmetatable(locked) == 'locked'); "
            "assert(not pcall(setmetatable, locked, {})); "
            "assert(not pcall(setmetatable, locked, nil)); "
            "assert(not pcall(setmetatable, 1, {})); "
            "assert(not pcall(setmetatable, {}, 1)); "
            "assert(not pcall(setmetatable, {})); "
            /* Every non-nil __gc marks an object in Lua, including false. */
            "local target = setmetatable({}, mt); "
            "for _, value in ipairs({record_finalizer, false, 0, 'gc', {}}) do "
            "local success, err = pcall(setmetatable, target, {__gc = value}); "
            "assert(not success and string.find(err, '__gc', 1, true)); "
            "assert(getmetatable(target) == mt) end; "
            /* Adding __gc later does not register existing objects. Reattaching
               that shared metatable to any object must nevertheless fail. */
            "mt.__gc = record_finalizer; "
            "assert(not pcall(setmetatable, b, mt)); "
            "assert(not pcall(setmetatable, {}, mt)); "
            "assert(setmetatable(target, nil) == target); "
            "b = nil; target = nil; collectgarbage('collect'); "
            "mt.__gc = nil; local c = setmetatable({}, mt); "
            "mt.__gc = record_finalizer; keep_until_close = c; "
            /* Inherited __gc is not a finalizer field and should be allowed. */
            "local inherited = setmetatable({}, {__index = {__gc = record_finalizer}}); "
            "local d = setmetatable({}, inherited); "
            "d = nil; collectgarbage('collect'); "
            "assert(not pcall(assert(load('return setmetatable({}, {__gc = function() end})'))))";
        if (luaL_dostring(L, checks) != LUA_OK) {
            fprintf(stderr, "Lua library policy failed: %s\n", lua_tostring(L, -1));
            lua_close(L);
            return 1;
        }
        lua_close(L);
        if (finalizer_calls != 0) {
            fprintf(stderr, "A cartridge finalizer ran during collection or closure\n");
            return 1;
        }
    }
    return 0;
}
