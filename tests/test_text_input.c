/* Real SDL event queue and native bounded input, independent of Lua. */
#include "sb_input.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Text input %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void key(SDL_Scancode scan, bool down, bool repeat) {
    SDL_Event e = {0}; e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    e.key.keysym.scancode = scan; e.key.repeat = repeat; SDL_PushEvent(&e);
}
static void text(const char *s) {
    SDL_Event e = {0}; e.type = SDL_TEXTINPUT;
    snprintf(e.text.text, sizeof(e.text.text), "%s", s); SDL_PushEvent(&e);
}
static void focus(Uint8 kind) {
    SDL_Event e = {0}; e.type=SDL_WINDOWEVENT; e.window.event=kind; SDL_PushEvent(&e);
}
static void tap(SDL_Scancode scan) { key(scan,true,false); key(scan,false,false); }
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)==0);
    sb_input_t *in=sb_input_create(); CHECK(in);
    sb_text_batch_t b;
    tap(SDL_SCANCODE_RETURN); CHECK(sb_input_poll(in)); sb_input_begin_update(in);
    CHECK(sb_input_pressed(in,0,SB_BUTTON_START)); sb_input_end_update(in);
    tap(SDL_SCANCODE_ESCAPE); CHECK(sb_input_poll(in)); sb_input_begin_update(in);
    CHECK(sb_input_pressed(in,0,SB_BUTTON_START)); sb_input_end_update(in);
    key(SDL_SCANCODE_J,true,false); CHECK(sb_input_poll(in)); sb_input_begin_update(in);
    CHECK(sb_input_pressed(in,0,SB_BUTTON_A));
    sb_input_text_mode(in,true); CHECK(sb_input_text_active(in));
    CHECK(SDL_IsTextInputActive());
    CHECK(!sb_input_down(in,0,SB_BUTTON_A) && !sb_input_pressed(in,0,SB_BUTTON_A));
    text("j"); key(SDL_SCANCODE_J,true,true); CHECK(sb_input_poll(in));
    sb_input_end_update(in); sb_input_begin_update(in); sb_input_text_read(in,&b); CHECK(b.count==0);
    sb_input_end_update(in); key(SDL_SCANCODE_J,false,false); CHECK(sb_input_poll(in));
    text("aB!"); tap(SDL_SCANCODE_LEFT); text("c"); tap(SDL_SCANCODE_RETURN);
    CHECK(sb_input_poll(in)); CHECK(!sb_input_down(in,0,SB_BUTTON_START));
    sb_input_text_read(in,&b); CHECK(b.count==0); /* no update yet */
    CHECK(sb_input_poll(in)); sb_input_begin_update(in);
    CHECK(!sb_input_pressed(in,0,SB_BUTTON_START));
    sb_input_text_read(in,&b); CHECK(b.count==6 && b.status==SB_TEXT_OK);
    CHECK(b.events[0].character=='a' && b.events[1].character=='B' && b.events[2].character=='!');
    CHECK(b.events[3].kind==SB_TEXT_LEFT && b.events[4].character=='c' && b.events[5].kind==SB_TEXT_SUBMIT);
    sb_input_text_read(in,&b); CHECK(!b.count); sb_input_end_update(in);
    sb_input_begin_update(in); sb_input_text_read(in,&b); CHECK(!b.count); sb_input_end_update(in);
    /* Native key repeat edits, but held Return never resubmits. */
    key(SDL_SCANCODE_BACKSPACE,true,false); key(SDL_SCANCODE_BACKSPACE,true,true);
    key(SDL_SCANCODE_RETURN,true,false); key(SDL_SCANCODE_RETURN,true,true);
    CHECK(sb_input_poll(in)); sb_input_begin_update(in); sb_input_text_read(in,&b);
    CHECK(b.count==3 && b.events[2].kind==SB_TEXT_SUBMIT); sb_input_end_update(in);
    key(SDL_SCANCODE_BACKSPACE,false,false); key(SDL_SCANCODE_RETURN,false,false);
    CHECK(sb_input_poll(in));
    /* Unsupported text rejects the whole batch, including a pending submit. */
    text("keep"); text("\xc3\xa9"); tap(SDL_SCANCODE_RETURN);
    CHECK(sb_input_poll(in)); sb_input_begin_update(in); sb_input_text_read(in,&b);
    CHECK(!b.count && b.status==SB_TEXT_UNSUPPORTED); sb_input_end_update(in);
    for(int i=0;i<128;i++) text("x");
    CHECK(sb_input_poll(in)); sb_input_begin_update(in); sb_input_text_read(in,&b);
    CHECK(b.count==128 && b.status==SB_TEXT_OK); sb_input_end_update(in);
    for(int i=0;i<129;i++) text("x");
    tap(SDL_SCANCODE_RETURN); CHECK(sb_input_poll(in)); sb_input_begin_update(in); sb_input_text_read(in,&b);
    CHECK(!b.count && b.status==SB_TEXT_OVERFLOW); sb_input_end_update(in);
    text("unread"); CHECK(sb_input_poll(in)); sb_input_begin_update(in); sb_input_end_update(in);
    sb_input_begin_update(in); sb_input_text_read(in,&b); CHECK(!b.count && b.status==SB_TEXT_OK); sb_input_end_update(in);
    /* Fixed and extended IME composition are rejected, with no fake ASCII. */
    SDL_Event edit={0}; edit.type=SDL_TEXTEDITING;
    strcpy(edit.edit.text,"compose"); CHECK(SDL_PushEvent(&edit)==1);
    CHECK(sb_input_poll(in)); sb_input_begin_update(in); sb_input_text_read(in,&b);
    CHECK(!b.count && b.status==SB_TEXT_UNSUPPORTED); sb_input_end_update(in);
#if SDL_VERSION_ATLEAST(2,0,22)
    SDL_Event ext={0}; ext.type=SDL_TEXTEDITING_EXT;
    ext.editExt.text=SDL_strdup("compose"); CHECK(ext.editExt.text);
    CHECK(SDL_PushEvent(&ext)==1); CHECK(sb_input_poll(in));
    sb_input_begin_update(in); sb_input_text_read(in,&b);
    CHECK(!b.count && b.status==SB_TEXT_UNSUPPORTED); sb_input_end_update(in);
    ext.editExt.text=SDL_strdup("stale"); CHECK(ext.editExt.text);
    CHECK(SDL_PushEvent(&ext)==1);
    sb_input_text_mode(in,false); sb_input_text_mode(in,true);
    CHECK(sb_input_poll(in)); sb_input_begin_update(in); sb_input_text_read(in,&b);
    CHECK(!b.count && b.status==SB_TEXT_OK); sb_input_end_update(in);
#endif
    /* Idempotent begin must not discard useful input. */
    text("q"); CHECK(sb_input_poll(in)); sb_input_text_mode(in,true);
    sb_input_begin_update(in); sb_input_text_read(in,&b); CHECK(b.count==1); sb_input_end_update(in);
    /* Pausing discards both current/pending input, restores host keys, preserves request. */
    text("lost"); CHECK(sb_input_poll(in)); sb_input_begin_update(in);
    sb_input_text_suspend(in,true); sb_input_text_read(in,&b); CHECK(!b.count);
    CHECK(!sb_input_text_active(in)); CHECK(!SDL_IsTextInputActive()); sb_input_end_update(in);
    tap(SDL_SCANCODE_J); text("menu"); CHECK(sb_input_poll(in)); sb_input_begin_update(in);
    CHECK(sb_input_pressed(in,0,SB_BUTTON_A)); sb_input_end_update(in);
    sb_input_text_suspend(in,false); CHECK(sb_input_text_active(in));
    sb_input_begin_update(in); sb_input_text_read(in,&b); CHECK(!b.count); sb_input_end_update(in);
    /* Leaving text mode cannot turn held movement keys into gameplay. */
    key(SDL_SCANCODE_D,true,false); CHECK(sb_input_poll(in)); sb_input_text_mode(in,false);
    CHECK(!sb_input_down(in,0,SB_BUTTON_RIGHT));
    key(SDL_SCANCODE_D,true,true); CHECK(sb_input_poll(in)); CHECK(!sb_input_down(in,0,SB_BUTTON_RIGHT));
    key(SDL_SCANCODE_D,false,false); key(SDL_SCANCODE_D,true,false); CHECK(sb_input_poll(in));
    CHECK(sb_input_down(in,0,SB_BUTTON_RIGHT));
    sb_input_text_mode(in,true);
    key(SDL_SCANCODE_D,false,false); CHECK(sb_input_poll(in));
    text("old"); CHECK(sb_input_poll(in)); focus(SDL_WINDOWEVENT_FOCUS_LOST); CHECK(sb_input_poll(in));
    CHECK(!sb_input_text_active(in)); text("lost"); CHECK(sb_input_poll(in));
    focus(SDL_WINDOWEVENT_FOCUS_GAINED); CHECK(sb_input_poll(in)); CHECK(sb_input_text_active(in));
    sb_input_begin_update(in); sb_input_text_read(in,&b); CHECK(!b.count); sb_input_end_update(in);
    tap(SDL_SCANCODE_ESCAPE); CHECK(sb_input_poll(in)); sb_input_begin_update(in);
    CHECK(sb_input_pressed(in,0,SB_BUTTON_START)); sb_input_end_update(in);
#if SDL_VERSION_ATLEAST(2,0,14)
    int player=sb_input_controller_count(in);
    int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,0,SDL_CONTROLLER_BUTTON_MAX,0);
    if(device>=0) {
        CHECK(sb_input_poll(in));
        if(player<SB_MAX_PLAYERS && SDL_IsGameController(device)) {
            SDL_Event e={0}; e.cbutton.type=SDL_CONTROLLERBUTTONDOWN;
            e.cbutton.which=SDL_JoystickGetDeviceInstanceID(device); e.cbutton.button=SDL_CONTROLLER_BUTTON_A;
            CHECK(SDL_PushEvent(&e)==1); CHECK(sb_input_poll(in));
            sb_input_text_mode(in,false); sb_input_text_mode(in,true);
            sb_input_begin_update(in); CHECK(sb_input_pressed(in,player,SB_BUTTON_A));
            CHECK(sb_input_down(in,player,SB_BUTTON_A)); sb_input_end_update(in);
            e.cbutton.button=SDL_CONTROLLER_BUTTON_START; CHECK(SDL_PushEvent(&e)==1); CHECK(sb_input_poll(in));
            sb_input_begin_update(in); CHECK(sb_input_pressed(in,player,SB_BUTTON_START)); sb_input_end_update(in);
        }
        CHECK(SDL_JoystickDetachVirtual(device)==0); CHECK(sb_input_poll(in));
    }
#endif
    sb_input_text_reset(in); CHECK(!sb_input_text_active(in));
    text("idle"); CHECK(sb_input_poll(in)); sb_input_begin_update(in); sb_input_text_read(in,&b);
    CHECK(!b.count); sb_input_end_update(in);
    sb_input_destroy(in); SDL_Quit(); return 0;
}
