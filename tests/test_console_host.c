/* Exercise private host controls without starting SDL or invoking an OS action. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Console host test failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static void reset_host(bool console) {
    memset(&g_engine, 0, sizeof(g_engine));
    g_engine.is_running = true;
    g_engine.console_mode = console;
}

int main(void) {
    char *idle[] = {"sugarbee"};
    char *console[] = {"sugarbee", "--console"};
    char *direct[] = {"sugarbee", "--console", "game.vcart"};
    char *directory[] = {"sugarbee", "--console", "--cartridges", "/tmp/cart dir"};
    char *desktop_dir[] = {"sugarbee", "--cartridges", "/tmp/carts"};
    char *desktop_cart[] = {"sugarbee", "game.vcart"};
    char *missing[] = {"sugarbee", "--console", "--cartridges"};
    char *duplicate[] = {"sugarbee", "--console", "--console"};
    char *unknown[] = {"sugarbee", "--unknown"};
    char *empty[] = {"sugarbee", "--console", "--cartridges", ""};
    char *mixed[] = {"sugarbee", "--console", "game.vcart", "--cartridges", "/tmp/carts"};

    reset_host(false);
    CHECK(parse_host_arguments(1, idle) && !g_engine.console_mode);
    reset_host(false);
    CHECK(parse_host_arguments(2, console) && g_engine.console_mode);
    reset_host(false);
    CHECK(parse_host_arguments(3, direct) && g_engine.console_mode);
    CHECK(strcmp(g_engine.startup_cartridge_path, "game.vcart") == 0);
    reset_host(false);
    CHECK(parse_host_arguments(4, directory) && g_engine.console_mode);
    CHECK(strcmp(g_engine.cartridge_directory, "/tmp/cart dir") == 0);
    reset_host(false);
    CHECK(parse_host_arguments(3, desktop_dir) && !g_engine.console_mode);
    reset_host(false);
    CHECK(parse_host_arguments(2, desktop_cart) && !g_engine.console_mode);
    CHECK(!parse_host_arguments(3, missing));
    CHECK(!parse_host_arguments(3, duplicate));
    CHECK(!parse_host_arguments(2, unknown));
    CHECK(!parse_host_arguments(4, empty));
    CHECK(!parse_host_arguments(5, mixed));

    /* Console idle B is inert; opening consumes simultaneous A/direction/rescan. */
    reset_host(true);
    g_engine.cartridge_directory = "/tmp/carts";
    g_engine.cartridge_count = 3;
    g_engine.cartridge_selection = 1;
    handle_idle_host_input(false, true, false, false, false, false);
    CHECK(g_engine.is_running);
    CHECK(handle_idle_host_input(true, false, false, true, true, true));
    CHECK(g_engine.shutdown_confirmation && !g_engine.shutdown_selected);
    CHECK(!g_engine.cartridge_launch_requested && !g_engine.cartridge_rescan_requested);
    CHECK(g_engine.cartridge_selection == 1);
    /* No new press edges: held buttons do not retrigger anything. */
    for (int i = 0; i < 10; ++i)
        handle_idle_host_input(false, false, false, false, false, false);
    CHECK(g_engine.shutdown_confirmation && !g_engine.shutdown_selected);
    handle_idle_host_input(false, false, false, true, false, true);
    CHECK(g_engine.shutdown_selected && g_engine.cartridge_selection == 1);
    CHECK(!g_engine.cartridge_rescan_requested);
    handle_idle_host_input(false, true, false, false, true, false);
    CHECK(!g_engine.shutdown_confirmation && !g_engine.shutdown_test_reported);
    CHECK(g_engine.is_running && !g_engine.cartridge_launch_requested);

    handle_idle_host_input(true, false, false, false, false, false);
    CHECK(!g_engine.shutdown_selected); /* Reopen always selects Cancel. */
    handle_idle_host_input(false, false, false, true, true, false);
    CHECK(!g_engine.shutdown_confirmation && !g_engine.shutdown_test_reported);
    handle_idle_host_input(true, false, false, false, false, false);
    handle_idle_host_input(false, false, false, true, false, false);
    handle_idle_host_input(false, false, false, false, true, false);
    CHECK(g_engine.shutdown_test_reported && !g_engine.shutdown_confirmation);
    CHECK(g_engine.is_running && !g_engine.cartridge_launch_requested);
    handle_idle_host_input(true, false, false, false, false, false);
    CHECK(!g_engine.shutdown_test_reported && !g_engine.shutdown_selected);
    handle_idle_host_input(true, false, false, false, true, false);
    CHECK(!g_engine.shutdown_confirmation && !g_engine.shutdown_test_reported);

    /* Empty/direct-path idle hosts also have confirmation, without fake entries. */
    reset_host(true);
    handle_idle_host_input(true, false, false, false, false, false);
    CHECK(g_engine.shutdown_confirmation);
    handle_idle_host_input(false, true, false, false, false, false);
    g_engine.startup_cartridge_path = "game.vcart";
    handle_idle_host_input(false, false, false, false, true, false);
    CHECK(g_engine.cartridge_launch_requested);

    /* Active games and Guru are left to their existing controls. */
    reset_host(true);
    g_engine.cart_loaded = true;
    handle_idle_host_input(true, false, false, false, true, false);
    CHECK(!g_engine.shutdown_confirmation && !g_engine.cartridge_launch_requested);
    g_engine.cart_loaded = false;
    g_engine.guru_active = true;
    handle_idle_host_input(true, true, false, false, true, false);
    CHECK(!g_engine.shutdown_confirmation && g_engine.is_running);

    reset_host(false);
    handle_idle_host_input(true, false, false, false, false, false);
    CHECK(!g_engine.shutdown_confirmation);
    g_engine.cartridge_directory = "/tmp/carts";
    g_engine.cartridge_count = 2;
    handle_idle_host_input(false, false, false, true, false, false);
    CHECK(g_engine.cartridge_selection == 1);
    handle_idle_host_input(false, false, false, false, true, false);
    CHECK(g_engine.cartridge_launch_requested);
    handle_idle_host_input(false, true, false, false, false, false);
    CHECK(!g_engine.is_running);
    puts("Console host controls passed");
    return 0;
}
