/* Exercise production Lua bindings, module loader, lifecycle and demo callbacks. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Text host %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void key(SDL_Scancode scan, bool down, bool repeat) {
    SDL_Event e={0}; e.type=down?SDL_KEYDOWN:SDL_KEYUP;
    e.key.keysym.scancode=scan; e.key.repeat=repeat; SDL_PushEvent(&e);
}
static void text(const char *s) {
    SDL_Event e={0}; e.type=SDL_TEXTINPUT;
    snprintf(e.text.text,sizeof(e.text.text),"%s",s); SDL_PushEvent(&e);
}
static bool run_update(void) {
    if(!sb_input_poll(g_engine.input)) return false;
    sb_input_begin_update(g_engine.input);
    call_lua_global_function("update",true,1.0/60);
    sb_input_end_update(g_engine.input);
    return !g_engine.guru_active;
}
static bool lua(const char *source) {
    int status=luaL_dostring(g_engine.L,source);
    if(status!=LUA_OK) fprintf(stderr,"%s\n",lua_tostring(g_engine.L,-1));
    return status==LUA_OK;
}
int main(int argc,char **argv) {
    CHECK(argc==2);
    CHECK(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)==0);
    g_engine.graphics=sb_graphics_create("Text entry test",1280,720); CHECK(g_engine.graphics);
    g_engine.input=sb_input_create(); CHECK(g_engine.input);
    g_engine.is_running=true;
    CHECK(start_cartridge(argv[1]));
    CHECK(!sb_input_text_active(g_engine.input));
    CHECK(lua("assert(not pcall(sugarbee.text_begin)); assert(not pcall(sugarbee.text_end)); assert(not pcall(sugarbee.text_events))"));
    CHECK(lua("demo_update=update; demo_draw=draw; "
        "function update() "
        "assert(not pcall(sugarbee.text_begin,1)); assert(not pcall(sugarbee.text_end,false)); "
        "assert(not pcall(sugarbee.text_events,nil)); "
        "assert(select('#',sugarbee.text_begin())==0); "
        "assert(select('#',sugarbee.text_end())==0); "
        "assert(select('#',sugarbee.text_events())==2); "
        "local e,s=sugarbee.text_events(); assert(#e==0 and s==nil) end"));
    CHECK(run_update()); CHECK(lua_gettop(g_engine.L)==0);
    CHECK(lua("function draw() assert(not pcall(sugarbee.text_events)); "
        "assert(not pcall(sugarbee.text_begin)); assert(not pcall(sugarbee.text_end)) end"));
    call_lua_global_function("draw",false,0); CHECK(!g_engine.guru_active);
    CHECK(lua("function init() sugarbee.text_begin() end"));
    call_lua_global_function("init",false,0); CHECK(!g_engine.guru_active);
    CHECK(sb_input_text_active(g_engine.input));
    CHECK(lua("function update() "
        "local events,status=sugarbee.text_events(); assert(status==nil and #events==4); "
        "assert(events[1].type=='text' and events[1].text=='A'); "
        "assert(events[2].type=='text' and events[2].text=='!'); "
        "assert(events[3].type=='left' and events[4].type=='submit'); "
        "local second,err=sugarbee.text_events(); assert(#second==0 and err==nil); "
        "sugarbee.text_end() end"));
    text("A!"); key(SDL_SCANCODE_LEFT,true,false); key(SDL_SCANCODE_LEFT,false,false);
    key(SDL_SCANCODE_RETURN,true,false); CHECK(run_update());
    CHECK(!sb_input_text_active(g_engine.input));
    CHECK(!sb_input_down(g_engine.input,0,SB_BUTTON_START));
    key(SDL_SCANCODE_RETURN,false,false); CHECK(sb_input_poll(g_engine.input));
    CHECK(lua("function update() "
        "local e,s=sugarbee.text_events(); assert(#e==0 and s=='unsupported_text'); "
        "e,s=sugarbee.text_events(); assert(#e==0 and s==nil) end"));
    sb_input_text_mode(g_engine.input,true); text("\xc3\xa9"); CHECK(run_update());
    /* Full independent session restores the demo and empties all input state. */
    end_cartridge(); CHECK(!sb_input_text_active(g_engine.input));
    CHECK(start_cartridge(argv[1]));
    CHECK(lua("local m=require('ui.textline'); local original=m.new; "
        "m.new=function() observed_editor=original(); return observed_editor end"));
    key(SDL_SCANCODE_J,true,false); CHECK(run_update());
    CHECK(sb_input_text_active(g_engine.input));
    key(SDL_SCANCODE_J,false,false); CHECK(run_update());
    text("abc"); CHECK(run_update());
    call_lua_global_function("draw",false,0); CHECK(!g_engine.guru_active);
    /* Check the actual editor through its cached module, without exposing demo locals. */
    CHECK(lua("assert(observed_editor.text=='abc' and observed_editor.cursor==3); "
        "original_update=update; function update() "
        "original_update(); assert(sugarbee.btn('left')==false) end"));
    text("must discard"); CHECK(sb_input_poll(g_engine.input)); sb_input_begin_update(g_engine.input);
    set_cartridge_paused(true); CHECK(g_engine.paused && !sb_input_text_active(g_engine.input));
    sb_input_end_update(g_engine.input);
    text("menu typing"); key(SDL_SCANCODE_K,true,false); CHECK(sb_input_poll(g_engine.input));
    sb_input_begin_update(g_engine.input); CHECK(sb_input_pressed(g_engine.input,0,SB_BUTTON_B));
    set_cartridge_paused(false); sb_input_end_update(g_engine.input);
    CHECK(sb_input_text_active(g_engine.input));
    key(SDL_SCANCODE_K,false,false); CHECK(run_update());
    CHECK(lua("assert(observed_editor.text=='abc' and observed_editor.cursor==3)"));
    key(SDL_SCANCODE_LEFT,true,false); key(SDL_SCANCODE_LEFT,false,false);
    text("X"); key(SDL_SCANCODE_DELETE,true,false); key(SDL_SCANCODE_DELETE,false,false);
    CHECK(run_update()); call_lua_global_function("draw",false,0); CHECK(!g_engine.guru_active);
    CHECK(lua("assert(observed_editor.text=='abX' and observed_editor.cursor==3)"));
    key(SDL_SCANCODE_RETURN,true,false); CHECK(run_update()); CHECK(!sb_input_text_active(g_engine.input));
    key(SDL_SCANCODE_RETURN,true,true); CHECK(sb_input_poll(g_engine.input));
    sb_input_begin_update(g_engine.input); CHECK(!sb_input_pressed(g_engine.input,0,SB_BUTTON_START)); sb_input_end_update(g_engine.input);
    key(SDL_SCANCODE_RETURN,false,false); CHECK(sb_input_poll(g_engine.input));
    end_cartridge(); CHECK(start_cartridge(argv[1])); CHECK(!sb_input_text_active(g_engine.input));
    CHECK(lua("local m=require('ui.textline'); local original=m.new; "
        "m.new=function() observed_editor=original(); return observed_editor end"));
    key(SDL_SCANCODE_J,true,false); CHECK(run_update());
    key(SDL_SCANCODE_J,false,false); CHECK(run_update());
    for(int pass=0;pass<2;++pass) {
        for(int i=0;i<128;++i) text("x");
        CHECK(run_update());
    }
    CHECK(lua("assert(#observed_editor.text==256 and observed_editor.cursor==256)"));
    text("q"); key(SDL_SCANCODE_RETURN,true,false); key(SDL_SCANCODE_RETURN,false,false);
    CHECK(run_update()); CHECK(sb_input_text_active(g_engine.input));
    CHECK(lua("assert(#observed_editor.text==256 and observed_editor.cursor==256)"));
    call_lua_global_function("draw",false,0); CHECK(!g_engine.guru_active);
    key(SDL_SCANCODE_BACKSPACE,true,false); key(SDL_SCANCODE_BACKSPACE,false,false);
    key(SDL_SCANCODE_RETURN,true,false); key(SDL_SCANCODE_RETURN,false,false);
    CHECK(run_update()); CHECK(!sb_input_text_active(g_engine.input));
    CHECK(lua("assert(#observed_editor.text==255)"));
    CHECK(lua("function update() sugarbee.text_begin(); error('intentional failure') end"));
    CHECK(!run_update()); CHECK(g_engine.guru_active && !sb_input_text_active(g_engine.input));
    key(SDL_SCANCODE_K,true,false); CHECK(sb_input_poll(g_engine.input));
    sb_input_begin_update(g_engine.input); CHECK(sb_input_pressed(g_engine.input,0,SB_BUTTON_B)); sb_input_end_update(g_engine.input);
    key(SDL_SCANCODE_K,false,false); CHECK(sb_input_poll(g_engine.input));
    end_cartridge(); g_engine.guru_active=false;
    /* Startup failure after enabling text input must also release host controls. */
    CHECK(start_cartridge(argv[1]));
    CHECK(lua("function init() sugarbee.text_begin(); error('intentional init failure') end"));
    call_lua_global_function("init",false,0);
    CHECK(g_engine.guru_active && !sb_input_text_active(g_engine.input));
    end_cartridge(); g_engine.guru_active=false;
    CHECK(start_cartridge(argv[1]));
    CHECK(lua("function update() sugarbee.text_begin() end; "
        "function shutdown() assert(not pcall(sugarbee.text_begin)); assert(not pcall(sugarbee.text_events)) end"));
    CHECK(run_update()); CHECK(sb_input_text_active(g_engine.input));
    end_cartridge(); CHECK(!sb_input_text_active(g_engine.input));
    CHECK(g_engine.L==NULL);
    sb_input_destroy(g_engine.input); sb_graphics_destroy(g_engine.graphics);
    SDL_Quit(); return 0;
}
