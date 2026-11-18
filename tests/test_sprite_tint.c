/* Pixel evidence for multiplicative tint, alpha composition and state isolation. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Tint line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int pixel(SDL_Renderer *r,int x,int y,int red,int green,int blue,int tolerance) {
    Uint8 p[4]; SDL_Rect area={x,y,1,1};
    if (SDL_RenderReadPixels(r,&area,SDL_PIXELFORMAT_RGBA32,p,4)) return 0;
    if (abs(p[0]-red)<=tolerance && abs(p[1]-green)<=tolerance && abs(p[2]-blue)<=tolerance) return 1;
    fprintf(stderr,"Pixel %d,%d: %u,%u,%u expected %d,%d,%d\n",x,y,p[0],p[1],p[2],red,green,blue);
    return 0;
}
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    sb_graphics_t *g=sb_graphics_create("Tint",32,24); CHECK(g);
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1)); CHECK(r);
    const uint8_t rgba[]={255,255,255,255,128,128,128,255,0,0,0,255,
                          255,255,255,128,255,255,255,0,200,100,50,255};
    sb_texture_t t=sb_graphics_texture_rgba(g,6,1,rgba,sizeof rgba); CHECK(t);
    sb_texture_t other=sb_graphics_texture_rgba(g,6,1,rgba,sizeof rgba); CHECK(other);
    /* Initialization must default to white, not calloc's zero channels. */
    sb_graphics_clear(g,0,0,0); CHECK(sb_graphics_sprite(g,t,0,0));
    CHECK(pixel(r,0,0,255,255,255,0));
    sb_graphics_sprite_tint(g,200,100,50,255);
    sb_graphics_clear(g,0,0,0); /* Does not reset tint. */
    CHECK(sb_graphics_sprite(g,t,0,0));
    CHECK(pixel(r,0,0,200,100,50,0));
    CHECK(pixel(r,1,0,100,50,25,1));
    CHECK(pixel(r,2,0,0,0,0,0));
    CHECK(pixel(r,3,0,100,50,25,2));
    CHECK(pixel(r,4,0,0,0,0,0));
    CHECK(pixel(r,5,0,157,39,10,1));
    /* Same texture in several colors, second texture, then original pixels. */
    sb_graphics_sprite_tint(g,0,255,0,255); CHECK(sb_graphics_sprite(g,t,0,1));
    CHECK(pixel(r,0,1,0,255,0,0)); CHECK(pixel(r,0,0,200,100,50,0));
    CHECK(sb_graphics_sprite(g,other,0,2)); CHECK(pixel(r,0,2,0,255,0,0));
    sb_graphics_sprite_tint_reset(g); CHECK(sb_graphics_sprite(g,t,0,3));
    CHECK(pixel(r,5,3,200,100,50,0));
    /* Source alpha and draw opacity multiply before background blending.
     * Small tolerance allows SDL backend integer rounding, not different colors. */
    sb_graphics_clear(g,20,40,60); sb_graphics_sprite_tint(g,255,255,255,128);
    CHECK(sb_graphics_sprite(g,t,0,0));
    CHECK(pixel(r,0,0,138,148,158,2));
    CHECK(pixel(r,3,0,79,94,109,2));
    CHECK(pixel(r,4,0,20,40,60,0));
    sb_graphics_sprite_tint(g,0,0,0,0); CHECK(sb_graphics_sprite(g,t,0,1));
    CHECK(pixel(r,0,1,20,40,60,0));
    CHECK(!sb_graphics_sprite(g,0,0,0));
    CHECK(!sb_graphics_sprite_region(g,t,0,0,6,0,1,1,1,1,false,false));
    /* Tint never changes primitive/text colors. */
    sb_graphics_rect(g,0,2,1,1,255,255,255,true);
    sb_graphics_text(g,"F",8,0,255,255,255,1);
    CHECK(pixel(r,0,2,255,255,255,0)); CHECK(pixel(r,9,5,255,255,255,0));
    /* Source crop + flip + scale, camera and screen clip all compose. */
    sb_graphics_clear(g,0,0,0); sb_graphics_sprite_tint(g,255,0,0,255);
    sb_graphics_camera(g,100,200); CHECK(sb_graphics_clip(g,3,4,3,2));
    CHECK(sb_graphics_sprite_region(g,t,102,204,0,0,2,1,4,2,true,true));
    CHECK(pixel(r,2,4,0,0,0,0)); CHECK(pixel(r,3,4,128,0,0,0));
    CHECK(pixel(r,4,4,255,0,0,0)); CHECK(pixel(r,5,5,255,0,0,0));
    CHECK(pixel(r,6,5,0,0,0,0));
    CHECK(sb_graphics_clip_reset(g)); sb_graphics_camera_reset(g);
    /* Explicit, frame, presentation and cartridge resets apply to reused textures. */
    for (int reset=0;reset<4;++reset) {
        sb_graphics_sprite_tint(g,0,0,0,0);
        if (reset==0) sb_graphics_sprite_tint_reset(g);
        if (reset==1) CHECK(sb_graphics_begin_game_frame(g));
        if (reset==2) CHECK(sb_graphics_show_game_frame(g));
        if (reset==3) {
            sb_graphics_clear_cartridge_textures(g);
            CHECK(!sb_graphics_sprite(g,t,0,0));
            t=sb_graphics_texture_rgba(g,6,1,rgba,sizeof rgba); CHECK(t);
        }
        sb_graphics_clear(g,0,0,0); CHECK(sb_graphics_sprite(g,t,0,0));
        CHECK(pixel(r,0,0,255,255,255,0));
    }
    sb_graphics_destroy(g); SDL_Quit(); return 0;
}
