/* Independent pixel-membership oracle: no square roots or row extents. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#define W 80
#define H 64
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int inside(int64_t dx, int64_t dy, int radius) {
    if (dx < -(int64_t)radius || dx > radius ||
        dy < -(int64_t)radius || dy > radius) return 0;
    /* Each term <= INT_MAX squared; their sum fits signed 64 bits. */
    return dx*dx + dy*dy <= (int64_t)radius*radius;
}
static int compare(SDL_Renderer *renderer, int64_t cx, int64_t cy, int radius,
                   int filled, int clipped) {
    Uint8 pixels[W*H*4];
    if (SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,pixels,W*4)) return 0;
    for (int y=0;y<H;++y) for (int x=0;x<W;++x) {
        int64_t dx=x-cx, dy=y-cy;
        int on=inside(dx,dy,radius);
        if (!filled) on=on && (!inside(dx-1,dy,radius) || !inside(dx+1,dy,radius) ||
                              !inside(dx,dy-1,radius) || !inside(dx,dy+1,radius));
        if (clipped && (x<11 || x>=58 || y<7 || y>=46)) on=0;
        int i=(y*W+x)*4, value=on?255:0;
        if (pixels[i]!=value || pixels[i+1]!=value || pixels[i+2]!=value) {
            fprintf(stderr,"Pixel %d,%d radius %d fill %d clip %d\n",x,y,radius,filled,clipped);
            return 0;
        }
    }
    return 1;
}
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    sb_graphics_t *g=sb_graphics_create("Circles",W,H); CHECK(g);
    SDL_Renderer *renderer=SDL_GetRenderer(SDL_GetWindowFromID(1)); CHECK(renderer);
    CHECK(sb_graphics_begin_game_frame(g));
    CHECK(!sb_graphics_circle(NULL,0,0,0,255,255,255));
    CHECK(!sb_graphics_circle_fill(NULL,0,0,0,255,255,255));
    CHECK(!sb_graphics_circle(g,0,0,-1,255,255,255));
    CHECK(!sb_graphics_circle_fill(g,0,0,-1,255,255,255));
    sb_graphics_sprite_tint(g,0,0,0,0);
    /* Every small radius, centered and crossing edges. Symmetric membership
     * also verifies tiny-circle shape and exact outline/fill correspondence. */
    for (int radius=0;radius<=65;++radius) for (int position=0;position<3;++position)
    for (int filled=0;filled<2;++filled) for (int clipped=0;clipped<2;++clipped) {
        int x=position==0?40:position==1?-3:79;
        int y=position==0?32:position==1?7:65;
        CHECK(sb_graphics_clip(g,clipped?11:0,clipped?7:0,clipped?47:W,clipped?39:H));
        sb_graphics_clear(g,0,0,0);
        CHECK(filled ? sb_graphics_circle_fill(g,x,y,radius,255,255,255)
                     : sb_graphics_circle(g,x,y,radius,255,255,255));
        CHECK(compare(renderer,x,y,radius,filled,clipped));
    }
    /* Large radii, tangencies, widely translated origins, and cancellation
     * near each coordinate limit. These must finish within the test timeout. */
    const int cases[][5]={
        {0,0,INT_MAX,0,0}, {INT_MAX,0,INT_MAX,0,0},
        {-INT_MAX,0,INT_MAX,0,0}, {0,INT_MAX,INT_MAX,0,0},
        {INT_MIN,INT_MAX,INT_MAX,INT_MAX,INT_MIN},
        {INT_MAX,INT_MIN,INT_MAX,INT_MIN,INT_MAX},
        {INT_MIN+40,INT_MIN+32,25,INT_MIN,INT_MIN},
        {INT_MAX-40,INT_MAX-32,25,INT_MAX-80,INT_MAX-64},
        {INT_MIN,INT_MIN,INT_MAX,0,0},
        {INT_MAX,INT_MAX,INT_MAX,0,0}
    };
    for (unsigned k=0;k<sizeof cases/sizeof cases[0];++k)
    for (int filled=0;filled<2;++filled) for (int clipped=0;clipped<2;++clipped) {
        const int *c=cases[k];
        sb_graphics_camera(g,c[3],c[4]);
        CHECK(sb_graphics_clip(g,clipped?11:0,clipped?7:0,clipped?47:W,clipped?39:H));
        sb_graphics_clear(g,0,0,0);
        CHECK(filled ? sb_graphics_circle_fill(g,c[0],c[1],c[2],255,255,255)
                     : sb_graphics_circle(g,c[0],c[1],c[2],255,255,255));
        CHECK(compare(renderer,(int64_t)c[0]-c[3],(int64_t)c[1]-c[4],c[2],filled,clipped));
    }
    sb_graphics_camera_reset(g);
    CHECK(sb_graphics_clip(g,0,0,0,0));
    sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_circle_fill(g,0,0,INT_MAX,255,255,255));
    CHECK(compare(renderer,-100,-100,0,1,0)); /* Empty image. */
    CHECK(sb_graphics_clip_reset(g));
    CHECK(sb_graphics_circle_fill(g,20,20,10,20,40,60));
    CHECK(sb_graphics_circle(g,20,20,10,80,90,100));
    Uint8 p[4]; SDL_Rect point={20,20,1,1};
    CHECK(SDL_RenderReadPixels(renderer,&point,SDL_PIXELFORMAT_RGBA32,p,4)==0);
    CHECK(p[0]==20 && p[1]==40 && p[2]==60);
    point.x=30;
    CHECK(SDL_RenderReadPixels(renderer,&point,SDL_PIXELFORMAT_RGBA32,p,4)==0);
    CHECK(p[0]==80 && p[1]==90 && p[2]==100);
    sb_graphics_destroy(g); SDL_Quit(); return 0;
}
