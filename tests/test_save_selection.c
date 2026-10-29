/* Test the real host parser and selector without SDL or storage operations. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Save selection failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static bool parse(int argc, char **argv) {
    memset(&g_engine, 0, sizeof(g_engine));
    return parse_host_arguments(argc, argv);
}

int main(void) {
    char *idle[] = {"sugarbee", "--save-dir", "/chosen"};
    char *direct[] = {"sugarbee", "--save-dir", "./save files", "game.vcart"};
    char *directory[] = {"sugarbee", "--console", "--save-dir", "/save files",
                         "--cartridges", "/games"};
    char *console[] = {"sugarbee", "--console", "--save-dir", "/chosen"};
    char *plain[] = {"sugarbee"};
    CHECK(parse(3, idle) && strcmp(g_engine.save_directory_override, "/chosen") == 0);
    CHECK(parse(4, direct) && strcmp(g_engine.startup_cartridge_path, "game.vcart") == 0);
    CHECK(strcmp(g_engine.save_directory_override, "./save files") == 0);
    CHECK(parse(6, directory) && g_engine.console_mode);
    CHECK(strcmp(g_engine.cartridge_directory, "/games") == 0);
    CHECK(parse(4, console) && g_engine.console_mode);
    CHECK(parse(1, plain) && !g_engine.save_directory_override);

    char *missing[] = {"sugarbee", "--save-dir"};
    char *empty[] = {"sugarbee", "--save-dir", ""};
    char *duplicate[] = {"sugarbee", "--save-dir", "one", "--save-dir", "two"};
    char *option_value[] = {"sugarbee", "--save-dir", "--console"};
    char *late[] = {"sugarbee", "game.vcart", "--save-dir", "one"};
    char *late_dir[] = {"sugarbee", "--cartridges", "/games", "--save-dir", "one"};
    char *late_console[] = {"sugarbee", "--save-dir", "one", "--console"};
    CHECK(!parse(2, missing)); CHECK(!parse(3, empty));
    CHECK(!parse(5, duplicate)); CHECK(!parse(3, option_value));
    CHECK(!parse(4, late)); CHECK(!parse(5, late_dir)); CHECK(!parse(4, late_console));

    CHECK(resolve_save_directory("/explicit", "/xdg", "/home", "/cwd"));
    CHECK(strcmp(g_engine.save_directory, "/explicit") == 0);
    CHECK(resolve_save_directory("./save files", "/xdg", "/home", "/launch"));
    CHECK(strcmp(g_engine.save_directory, "/launch/./save files") == 0);
    CHECK(resolve_save_directory("../saves", NULL, NULL, "/launch/game"));
    CHECK(strcmp(g_engine.save_directory, "/launch/game/../saves") == 0);
    CHECK(resolve_save_directory("saves", NULL, NULL, "/"));
    CHECK(strcmp(g_engine.save_directory, "/saves") == 0);
    CHECK(resolve_save_directory(NULL, "/xdg", "/home", NULL));
    CHECK(strcmp(g_engine.save_directory, "/xdg/sugarbee/saves") == 0);
    CHECK(resolve_save_directory(NULL, "/xdg/", "/home", NULL));
    CHECK(strcmp(g_engine.save_directory, "/xdg/sugarbee/saves") == 0);
    const char *invalid_xdg[] = {NULL, "", "relative"};
    for (size_t i = 0; i < sizeof(invalid_xdg)/sizeof(invalid_xdg[0]); ++i) {
        CHECK(resolve_save_directory(NULL, invalid_xdg[i], "/home", NULL));
        CHECK(strcmp(g_engine.save_directory, "/home/.local/share/sugarbee/saves") == 0);
    }
    const char *invalid_home[] = {NULL, "", "relative"};
    for (size_t i = 0; i < sizeof(invalid_home)/sizeof(invalid_home[0]); ++i) {
        CHECK(!resolve_save_directory(NULL, "relative", invalid_home[i], NULL));
        CHECK(!g_engine.save_directory[0] && g_engine.save_directory_error && !g_engine.guru_active);
    }
    CHECK(!resolve_save_directory("relative", "/xdg", "/home", NULL));
    CHECK(!g_engine.save_directory[0]); /* Explicit choice never falls back. */
    CHECK(!resolve_save_directory("", "/xdg", "/home", "/cwd"));
    char huge[SB_LAUNCHER_PATH_SIZE + 1];
    memset(huge, 'a', sizeof(huge)-1); huge[0] = '/'; huge[sizeof(huge)-1] = 0;
    CHECK(!resolve_save_directory(huge, "/xdg", "/home", NULL));
    CHECK(!resolve_save_directory(NULL, huge, "/home", NULL));
    CHECK(!g_engine.save_directory[0]);
    huge[SB_LAUNCHER_PATH_SIZE-1] = 0;
    CHECK(resolve_save_directory(huge, NULL, NULL, NULL)); /* Max stored path. */
    CHECK(!resolve_save_directory(NULL, huge, "/home", NULL)); /* Suffix won't fit. */
    CHECK(!g_engine.save_directory[0]);

    /* Real environment and cwd selection: no save destination is created. */
    char root[] = "/tmp/sugarbee-save-selection-XXXXXX";
    CHECK(mkdtemp(root));
    char previous[SB_LAUNCHER_PATH_SIZE], expected[SB_LAUNCHER_PATH_SIZE];
    CHECK(getcwd(previous, sizeof(previous)));
    CHECK(chdir(root) == 0);
    CHECK(setenv("XDG_DATA_HOME", "/unselected-xdg", 1) == 0);
    CHECK(setenv("HOME", "/unselected-home", 1) == 0);
    CHECK(parse(4, direct) && select_host_save_directory());
    snprintf(expected, sizeof(expected), "%s/./save files", root);
    CHECK(strcmp(g_engine.save_directory, expected) == 0 && access(expected, F_OK) != 0);
    CHECK(chdir(previous) == 0);
    CHECK(strcmp(g_engine.save_directory, expected) == 0); /* Captured, not borrowed. */
    CHECK(setenv("XDG_DATA_HOME", root, 1) == 0);
    CHECK(parse(1, plain) && select_host_save_directory());
    snprintf(expected, sizeof(expected), "%s/sugarbee/saves", root);
    CHECK(strcmp(g_engine.save_directory, expected) == 0 && access(expected, F_OK) != 0);
    CHECK(setenv("XDG_DATA_HOME", "relative", 1) == 0);
    CHECK(setenv("HOME", root, 1) == 0);
    CHECK(select_host_save_directory());
    snprintf(expected, sizeof(expected), "%s/.local/share/sugarbee/saves", root);
    CHECK(strcmp(g_engine.save_directory, expected) == 0 && access(expected, F_OK) != 0);
    CHECK(unsetenv("HOME") == 0 && unsetenv("XDG_DATA_HOME") == 0);
    CHECK(!select_host_save_directory() && !g_engine.guru_active);
    CHECK(rmdir(root) == 0); /* Selection created no intermediate directories either. */
    puts("Host save-directory selection checks passed");
    return 0;
}
