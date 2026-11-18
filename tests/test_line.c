/* Pixel comparison against a small, full-length incremental reference raster.
 * The production implementation skips directly to the visible major-axis span. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 160
#define H 48
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static unsigned char expected[W*H];
static int pixels_match(SDL_Renderer *r) {
    unsigned char actual[W*H*4];
    if (SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,actual,W*4)) return 0;
    for (int i=0;i<W*H;++i) {
        int value=expected[i] ? 255 : 0;
        if (actual[i*4]!=value || actual[i*4+1]!=value || actual[i*4+2]!=value) {
            fprintf(stderr,"Pixel %d,%d: %u,%u,%u expected %d\n",i%W,i/W,
                    actual[i*4],actual[i*4+1],actual[i*4+2],value); return 0;
        }
    }
    return 1;
}
/* Only called with small endpoints. Deliberately steps the entire line,
 * accumulating error; it neither jumps ahead nor uses production division. */
static void reference(int x1,int y1,int x2,int y2,int cx,int cy,int cw,int ch) {
    memset(expected,0,sizeof expected);
    int steep=abs(y2-y1)>abs(x2-x1), tmp;
    if (steep) { tmp=x1;x1=y1;y1=tmp; tmp=x2;x2=y2;y2=tmp; }
    if (x1>x2) { tmp=x1;x1=x2;x2=tmp; tmp=y1;y1=y2;y2=tmp; }
    int dx=x2-x1, dy=abs(y2-y1), sign=y2>=y1 ? 1 : -1;
    int error=dx/2, y=y1;
    for (int x=x1;x<=x2;++x) {
        int px=steep ? y : x, py=steep ? x : y;
        if (px>=0 && px<W && py>=0 && py<H && px>=cx && px<cx+cw && py>=cy && py<cy+ch)
            expected[py*W+px]=1;
        error+=dy;
        if (dx && error>=dx) { error-=dx; y+=sign; }
    }
}
static unsigned rng=12345;
static int coordinate(void) { rng=rng*1664525u+1013904223u; return (int)(rng%241)-60; }
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    sb_graphics_t *g=sb_graphics_create("Lines",W,H); CHECK(g);
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1)); CHECK(r);
    CHECK(sb_graphics_begin_game_frame(g));
    CHECK(!sb_graphics_line(NULL,0,0,1,1,255,255,255));
    /* All octants, tie slopes, endpoints, negative coords and clipping phase.
     * Tint zero must not suppress any primitive. */
    sb_graphics_sprite_tint(g,0,0,0,0);
    const int fixed[][4]={{1,1,1,1},{1,1,20,1},{1,1,1,20},{0,0,8,4},
        {0,8,8,4},{0,0,4,8},{8,0,4,8},{-17,-3,157,42},
        {-100,20,300,20},{70,-60,70,80},{-10,-10,-1,-1}};
    for (int k=0;k<600;++k) {
        int x1,y1,x2,y2;
        if (k<(int)(sizeof fixed/sizeof fixed[0])) {
            x1=fixed[k][0];y1=fixed[k][1];x2=fixed[k][2];y2=fixed[k][3];
        } else { x1=coordinate();y1=coordinate();x2=coordinate();y2=coordinate(); }
        for (int clipped=0;clipped<2;++clipped) {
            int cx=clipped ? 7 : 0, cy=clipped ? 5 : 0;
            int cw=clipped ? 51 : W, ch=clipped ? 29 : H;
            CHECK(sb_graphics_clip(g,cx,cy,cw,ch));
            reference(x1,y1,x2,y2,cx,cy,cw,ch);
            for (int reverse=0;reverse<2;++reverse) {
                sb_graphics_clear(g,0,0,0);
                CHECK(sb_graphics_line(g,reverse?x2:x1,reverse?y2:y1,
                    reverse?x1:x2,reverse?y1:y2,255,255,255));
                CHECK(pixels_match(r));
            }
        }
    }
    /* Camera translation, including origins near either integer limit. */
    const int cameras[]={-100,100,INT_MIN+200,INT_MAX-200};
    CHECK(sb_graphics_clip(g,7,5,51,29));
    reference(-17,-3,157,42,7,5,51,29);
    for (unsigned i=0;i<sizeof cameras/sizeof cameras[0];++i) {
        int c=cameras[i]; sb_graphics_camera(g,c,c); sb_graphics_clear(g,0,0,0);
        CHECK(sb_graphics_line(g,c-17,c-3,c+157,c+42,255,255,255));
        CHECK(pixels_match(r));
    }
    sb_graphics_camera_reset(g); CHECK(sb_graphics_clip_reset(g));
    /* Full-range crossing lines must finish within the CTest timeout. Their
     * expected pixels are analytic, independent of the small reference. */
    for (int mode=0;mode<6;++mode) {
        memset(expected,0,sizeof expected); sb_graphics_clear(g,0,0,0);
        if (mode==0) {
            CHECK(sb_graphics_line(g,INT_MIN,20,INT_MAX,20,255,255,255));
            for (int x=0;x<W;++x) expected[20*W+x]=1;
        } else if (mode==1) {
            CHECK(sb_graphics_line(g,30,INT_MAX,30,INT_MIN,255,255,255));
            for (int y=0;y<H;++y) expected[y*W+30]=1;
        } else if (mode==2) {
            CHECK(sb_graphics_line(g,INT_MIN,INT_MIN,INT_MAX,INT_MAX,255,255,255));
            for (int y=0;y<H;++y) expected[y*W+y]=1;
        } else if (mode==3) {
            sb_graphics_camera(g,INT_MAX,INT_MAX);
            CHECK(sb_graphics_line(g,INT_MIN,INT_MIN,INT_MAX,INT_MAX,255,255,255));
            expected[0]=1;
        } else if (mode==4) {
            sb_graphics_camera(g,INT_MIN,INT_MIN);
            CHECK(sb_graphics_line(g,INT_MIN,INT_MIN,INT_MAX,INT_MAX,255,255,255));
            for (int y=0;y<H;++y) expected[y*W+y]=1;
        } else {
            sb_graphics_camera(g,-10,-10);
            CHECK(sb_graphics_line(g,INT_MIN,INT_MAX,INT_MAX,INT_MIN,255,255,255));
            for (int y=0;y<20;++y) expected[y*W+19-y]=1;
        }
        CHECK(pixels_match(r)); sb_graphics_camera_reset(g);
    }
    memset(expected,0,sizeof expected); sb_graphics_clear(g,0,0,0);
    CHECK(sb_graphics_clip(g,0,0,0,0));
    CHECK(sb_graphics_line(g,INT_MIN,INT_MIN,INT_MAX,INT_MAX,255,255,255));
    CHECK(pixels_match(r)); CHECK(sb_graphics_clip_reset(g));
    CHECK(sb_graphics_line(g,INT_MIN,INT_MIN,INT_MIN,INT_MAX,255,255,255));
    CHECK(pixels_match(r));
    /* Explicit colors, single point, and last submission wins. */
    CHECK(sb_graphics_line(g,0,0,0,0,255,0,0));
    CHECK(sb_graphics_line(g,0,0,0,0,20,40,60));
    Uint8 p[4]; SDL_Rect pixel={0,0,1,1};
    CHECK(SDL_RenderReadPixels(r,&pixel,SDL_PIXELFORMAT_RGBA32,p,4)==0);
    CHECK(p[0]==20 && p[1]==40 && p[2]==60);
    sb_graphics_destroy(g); SDL_Quit(); return 0;
}
