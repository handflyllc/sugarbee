/* Actual cartridge modules, SDL text events, drawing and host pause lifecycle. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Panes host %d: %s\n",__LINE__,#x); return 1; } } while(0)
static bool lua(const char *source) {
    int status=luaL_dostring(g_engine.L,source);
    if(status!=LUA_OK) fprintf(stderr,"%s\n",lua_tostring(g_engine.L,-1));
    return status==LUA_OK;
}
static void text(const char *value) {
    SDL_Event e={0}; e.type=SDL_TEXTINPUT;
    snprintf(e.text.text,sizeof(e.text.text),"%s",value); SDL_PushEvent(&e);
}
static void key(SDL_Scancode scan) {
    SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.scancode=scan; SDL_PushEvent(&e);
    e.type=SDL_KEYUP; SDL_PushEvent(&e);
}
static bool tick(void) {
    if(!sb_input_poll(g_engine.input)) return false;
    sb_input_begin_update(g_engine.input);
    call_lua_global_function("update",true,1.0/60);
    sb_input_end_update(g_engine.input);
    call_lua_global_function("draw",false,0);
    return !g_engine.guru_active;
}
int main(int argc,char **argv) {
    CHECK(argc==2);
    CHECK(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)==0);
    g_engine.graphics=sb_graphics_create("Panes test",1280,720); CHECK(g_engine.graphics);
    g_engine.input=sb_input_create(); CHECK(g_engine.input);
    g_engine.is_running=true;
    CHECK(start_cartridge(argv[1])); CHECK(sb_input_text_active(g_engine.input));
    /* Observe game state through its cached factory, without exporting demo globals. */
    CHECK(lua("local m=require('app'); local new=m.new; m.new=function() observed=new(); return observed end"));
    call_lua_global_function("init",false,0); CHECK(!g_engine.guru_active);
    CHECK(tick());
    text("move right"); key(SDL_SCANCODE_RETURN); CHECK(tick());
    CHECK(lua("assert(observed.position==3 and observed.moves==1)"));
    const char *views[]={"view scene","view status","view console","view all"};
    for(size_t i=0;i<sizeof(views)/sizeof(views[0]);++i) {
        text(views[i]); key(SDL_SCANCODE_RETURN); CHECK(tick());
        CHECK(lua("assert(observed.position==3 and observed.moves==1 and observed.editor.text=='')"));
        CHECK(sb_input_text_active(g_engine.input));
    }
    text("move left"); key(SDL_SCANCODE_LEFT); CHECK(tick());
    CHECK(lua("assert(observed.editor.text=='move left' and observed.editor.cursor==8); saved_history=table.concat(observed.history,'|')"));
    set_cartridge_paused(true); CHECK(!sb_input_text_active(g_engine.input));
    text("discard me"); CHECK(sb_input_poll(g_engine.input));
    set_cartridge_paused(false); CHECK(sb_input_text_active(g_engine.input)); CHECK(tick());
    CHECK(lua("assert(observed.editor.text=='move left' and observed.editor.cursor==8); "
        "assert(observed.position==3 and table.concat(observed.history,'|')==saved_history)"));
    key(SDL_SCANCODE_RETURN); CHECK(tick());
    CHECK(lua("assert(observed.position==2 and observed.moves==2 and observed.editor.text=='')"));
    end_cartridge(); CHECK(!sb_input_text_active(g_engine.input));
    CHECK(start_cartridge(argv[1])); CHECK(sb_input_text_active(g_engine.input)); CHECK(tick());
    end_cartridge(); CHECK(!sb_input_text_active(g_engine.input));
    sb_input_destroy(g_engine.input); sb_graphics_destroy(g_engine.graphics); SDL_Quit();
    return 0;
}
