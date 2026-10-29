/* Private host operations tested through real lifecycle dispatch; the callbacks
   below are C test fixtures, not production Lua save bindings. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main

int __real_fsync(int fd);
int __real_renameat(int oldfd, const char *oldname, int newfd, const char *newname);
ssize_t __real_write(int fd, const void *data, size_t size);
int __real_mkdirat(int fd, const char *name, mode_t mode);
static bool fail_final_sync, fail_write, fail_preparation, replaced;
static unsigned mkdirs, writes;

int __wrap_mkdirat(int fd, const char *name, mode_t mode) {
    ++mkdirs;
    return __real_mkdirat(fd, name, mode);
}
int __wrap_fsync(int fd) {
    if ((fail_final_sync && replaced) || fail_preparation) { errno = EIO; return -1; }
    return __real_fsync(fd);
}
int __wrap_renameat(int oldfd, const char *oldname, int newfd, const char *newname) {
    int result = __real_renameat(oldfd, oldname, newfd, newname);
    if (result == 0) replaced = true;
    return result;
}
ssize_t __wrap_write(int fd, const void *data, size_t size) {
    ++writes;
    if (fail_write) { errno = ENOSPC; return -1; }
    return __real_write(fd, data, size);
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Host save check failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static const uint8_t initial[] = {0, 1, 255, 0, 7};
static const uint8_t updated[] = {4, 0, 3, 2};
static const uint8_t final[] = {9, 0, 8};
static unsigned init_calls, update_calls, shutdown_calls;
static bool callbacks_ok = true;

static bool matches(const uint8_t *expected, size_t size) {
    uint8_t *data = NULL; size_t length = 0;
    sb_save_result_t result = host_load_save(&data, &length);
    bool ok = result == SB_SAVE_OK && length == size &&
              (size == 0 || memcmp(data, expected, size) == 0);
    free(data); return ok;
}
static bool load_failure(sb_save_result_t expected) {
    uint8_t sentinel, *data = &sentinel; size_t length = 99;
    return host_load_save(&data, &length) == expected && data == NULL && length == 0;
}
static int test_init(lua_State *L) {
    (void)L; ++init_calls;
    callbacks_ok = callbacks_ok && !g_engine.cart_initialized &&
                   host_write_save(initial, sizeof(initial)) == SB_SAVE_OK &&
                   matches(initial, sizeof(initial));
    return 0;
}
static int test_update(lua_State *L) {
    (void)L; ++update_calls;
    callbacks_ok = callbacks_ok && g_engine.cart_initialized &&
                   host_write_save(updated, sizeof(updated)) == SB_SAVE_OK &&
                   matches(updated, sizeof(updated));
    return 0;
}
static int test_shutdown(lua_State *L) {
    (void)L; ++shutdown_calls;
    callbacks_ok = callbacks_ok && !g_engine.cart_initialized &&
                   host_write_save(final, sizeof(final)) == SB_SAVE_OK &&
                   matches(final, sizeof(final));
    return 0;
}
static bool new_session(uint8_t id) {
    g_engine.L = luaL_newstate();
    if (!g_engine.L) return false;
    open_cartridge_libraries(g_engine.L);
    register_sugarbee_api(g_engine.L);
    memset(&g_engine.current_cart, 0, sizeof(g_engine.current_cart));
    g_engine.current_cart.cart_uuid[15] = id;
    g_engine.cart_loaded = true;
    g_engine.cart_initialized = false;
    g_engine.guru_active = false;
    g_engine.execution_limit_exceeded = false;
    return true;
}
static bool remove_tree(const char *path) {
    DIR *dir = opendir(path); if (!dir) return false;
    bool ok = true; struct dirent *entry;
    while (ok && (entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[1024];
        int n = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        struct stat st;
        if (n < 0 || (size_t)n >= sizeof(child) || lstat(child, &st) != 0) { ok = false; break; }
        if (S_ISDIR(st.st_mode)) ok = remove_tree(child) && rmdir(child) == 0;
        else ok = unlink(child) == 0;
    }
    return closedir(dir) == 0 && ok;
}

int main(void) {
    char root[] = "/tmp/sugarbee-host-save-XXXXXX";
    CHECK(mkdtemp(root));
    char selected[512]; snprintf(selected, sizeof(selected), "%s/data/saves", root);
    CHECK(resolve_save_directory(selected, NULL, NULL, NULL));
    CHECK(host_write_save(initial, sizeof(initial)) == SB_SAVE_IO_ERROR);
    CHECK(load_failure(SB_SAVE_IO_ERROR));
    CHECK(!g_engine.guru_active && mkdirs == 0 && access(selected, F_OK) != 0);

    CHECK(new_session(1));
    /* No production Lua save API is exposed yet. */
    lua_getglobal(g_engine.L, "sugarbee");
    lua_getfield(g_engine.L, -1, "save"); CHECK(lua_isnil(g_engine.L, -1)); lua_pop(g_engine.L, 1);
    lua_getfield(g_engine.L, -1, "load_save"); CHECK(lua_isnil(g_engine.L, -1)); lua_pop(g_engine.L, 2);
    CHECK(load_failure(SB_SAVE_NOT_FOUND));
    CHECK(mkdirs == 0 && writes == 0 && access(selected, F_OK) != 0);
    g_engine.cart_loaded = false; /* top-level script is not an active loaded session */
    CHECK(host_write_save(initial, sizeof(initial)) == SB_SAVE_IO_ERROR);
    g_engine.cart_loaded = true;
    uint8_t oversized[SB_SAVE_MAX_BYTES + 1u] = {0};
    CHECK(host_write_save(oversized, sizeof(oversized)) == SB_SAVE_TOO_LARGE);
    CHECK(host_write_save(NULL, 1) == SB_SAVE_INVALID_ARGUMENT);
    CHECK(mkdirs == 0 && access(selected, F_OK) != 0);
    size_t length = 99;
    CHECK(host_load_save(NULL, &length) == SB_SAVE_INVALID_ARGUMENT && length == 0);

    lua_pushcfunction(g_engine.L, test_init); lua_setglobal(g_engine.L, "init");
    lua_pushcfunction(g_engine.L, test_update); lua_setglobal(g_engine.L, "update");
    lua_pushcfunction(g_engine.L, test_shutdown); lua_setglobal(g_engine.L, "shutdown");
    call_lua_global_function("init", false, 0);
    CHECK(init_calls == 1 && callbacks_ok && !g_engine.guru_active);
    g_engine.cart_initialized = true;
    call_lua_global_function("update", true, 1.0/60.0);
    CHECK(update_calls == 1 && callbacks_ok && !g_engine.guru_active);
    end_cartridge();
    CHECK(shutdown_calls == 1 && callbacks_ok && !g_engine.L);
    end_cartridge(); CHECK(shutdown_calls == 1); /* No second shutdown save. */
    CHECK(load_failure(SB_SAVE_IO_ERROR));

    CHECK(new_session(2)); CHECK(load_failure(SB_SAVE_NOT_FOUND));
    CHECK(host_write_save(initial, sizeof(initial)) == SB_SAVE_OK);
    end_cartridge();
    CHECK(new_session(1)); CHECK(matches(final, sizeof(final)));
    CHECK(host_write_save(NULL, 0) == SB_SAVE_OK && matches(NULL, 0));
    CHECK(host_write_save(final, sizeof(final)) == SB_SAVE_OK);

    /* Resolution occurs for each request; errors do not select another root. */
    CHECK(!resolve_save_directory(NULL, NULL, NULL, NULL));
    CHECK(load_failure(SB_SAVE_IO_ERROR));
    CHECK(host_write_save(initial, sizeof(initial)) == SB_SAVE_IO_ERROR);
    char ambiguous[512]; snprintf(ambiguous, sizeof(ambiguous), "%s/absent/../other", root);
    CHECK(resolve_save_directory(ambiguous, NULL, NULL, NULL));
    CHECK(load_failure(SB_SAVE_IO_ERROR));
    unsigned before = mkdirs;
    CHECK(host_write_save(initial, sizeof(initial)) == SB_SAVE_IO_ERROR && mkdirs == before);
    CHECK(resolve_save_directory(selected, NULL, NULL, NULL)); CHECK(matches(final, sizeof(final)));
    CHECK(host_write_save(oversized, sizeof(oversized)) == SB_SAVE_TOO_LARGE);
    CHECK(matches(final, sizeof(final)));

    fail_preparation = true; unsigned before_writes = writes;
    CHECK(host_write_save(initial, sizeof(initial)) == SB_SAVE_IO_ERROR && writes == before_writes);
    fail_preparation = false; CHECK(matches(final, sizeof(final)));
    fail_write = true;
    CHECK(host_write_save(initial, sizeof(initial)) == SB_SAVE_IO_ERROR);
    fail_write = false; CHECK(matches(final, sizeof(final)));
    replaced = false; fail_final_sync = true;
    CHECK(host_write_save(updated, sizeof(updated)) == SB_SAVE_DURABILITY_UNCERTAIN);
    fail_final_sync = false; CHECK(replaced && matches(updated, sizeof(updated)));
    CHECK(!g_engine.guru_active);

    /* Failed sessions are denied even if a limit error was caught before Guru. */
    g_engine.execution_limit_exceeded = true;
    CHECK(host_write_save(initial, sizeof(initial)) == SB_SAVE_IO_ERROR);
    CHECK(load_failure(SB_SAVE_IO_ERROR));
    g_engine.execution_limit_exceeded = false;
    CHECK(matches(updated, sizeof(updated)));
    g_engine.cart_initialized = true;
    lua_pushcfunction(g_engine.L, test_shutdown); lua_setglobal(g_engine.L, "shutdown");
    g_engine.guru_active = true;
    CHECK(host_write_save(initial, sizeof(initial)) == SB_SAVE_IO_ERROR);
    CHECK(load_failure(SB_SAVE_IO_ERROR));
    end_cartridge(); CHECK(shutdown_calls == 1); /* Guru skips shutdown callback. */
    CHECK(new_session(1)); CHECK(matches(updated, sizeof(updated)));
    end_cartridge();
    CHECK(new_session(2)); CHECK(matches(initial, sizeof(initial)));
    end_cartridge();
    CHECK(remove_tree(root) && rmdir(root) == 0);
    puts("Internal host save/load lifecycle checks passed");
    return 0;
}
