/* Small-coordinate oracle uses edge functions, not production scanline division. */
#include "sb_graphics.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#define W 64
#define H 48
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int oracle(const int *v, int x, int y) {
    int64_t area=(int64_t)(v[2]-v[0])*(v[5]-v[1])-(int64_t)(v[3]-v[1])*(v[4]-v[0]);
    if (!area) return 0;
    for (int i=0;i<3;++i) {
        int a=i, b=(i+1)%3;
        if (area<0) { int t=a; a=b; b=t; }
        int64_t dx=v[b*2]-v[a*2], dy=v[b*2+1]-v[a*2+1];
        int64_t e=dx*(2*y+1-2*v[a*2+1])-dy*(2*x+1-2*v[a*2]);
        if (e<0 || (e==0 && !(dy<0 || (dy==0 && dx>0)))) return 0;
    }
    return 1;
}
static int read_pixels(SDL_Renderer *r, Uint8 *pixels) {
    return SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,pixels,W*4)==0;
}
static unsigned rng=763;
static int coord(void) { rng=rng*1664525u+1013904223u; return (int)(rng%121)-30; }
static int draw(sb_graphics_t *g,const int *v,int r,int green,int b) {
    return sb_graphics_triangle_fill(g,v[0],v[1],v[2],v[3],v[4],v[5],
                                    (uint8_t)r,(uint8_t)green,(uint8_t)b);
}
int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
    sb_graphics_t *g=sb_graphics_create("Triangles",W,H); CHECK(g);
    SDL_Renderer *r=SDL_GetRenderer(SDL_GetWindowFromID(1)); CHECK(r);
    CHECK(sb_graphics_begin_game_frame(g));
    CHECK(!sb_graphics_triangle_fill(NULL,0,0,1,0,0,1,255,255,255));
    sb_graphics_sprite_tint(g,0,0,0,0);
    const int perms[][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    const int fixed[][6]={{0,0,1,0,0,1},{0,0,1,1,0,1},{4,4,44,4,20,40},
        {4,40,44,40,20,4},{0,0,20,20,40,40},{20,20,20,20,20,20},
        {20,5,20,5,30,30},{0,0,30,10,60,20},{0,5,30,5,60,5},
        {-10,0,64,48,64,0},{0,0,64,48,0,48}};
    Uint8 pixels[W*H*4], first[W*H*4];
    for (int k=0;k<200;++k) {
        int v[6];
        for (int i=0;i<6;++i) v[i]=k<(int)(sizeof fixed/sizeof fixed[0])?fixed[k][i]:coord();
        for (int clip=0;clip<2;++clip) for (int p=0;p<6;++p) {
            CHECK(sb_graphics_clip(g,clip?7:0,clip?9:0,clip?38:W,clip?24:H));
            sb_graphics_clear(g,0,0,0);
            int ordered[6];
            for (int i=0;i<3;++i) { ordered[2*i]=v[2*perms[p][i]]; ordered[2*i+1]=v[2*perms[p][i]+1]; }
            CHECK(draw(g,ordered,255,255,255)); CHECK(read_pixels(r,pixels));
            for (int y=0;y<H;++y) for (int x=0;x<W;++x) {
                int on=oracle(v,x,y) && (!clip || (x>=7 && x<45 && y>=9 && y<33));
                int j=(y*W+x)*4;
                CHECK(pixels[j]==(on?255:0) && pixels[j+1]==pixels[j] && pixels[j+2]==pixels[j]);
            }
        }
    }
    CHECK(sb_graphics_clip_reset(g));
    /* Both rectangle diagonals: differing colors reveal double ownership when
     * order reverses. Union must match the existing rectangle primitive. */
    for (int diagonal=0;diagonal<2;++diagonal) {
        int a[6]={10,10,30,10,diagonal?10:30,20};
        int b[6]={diagonal?30:10,10,30,20,10,20};
        for (int reverse=0;reverse<2;++reverse) {
            sb_graphics_clear(g,0,0,0);
            if (reverse) { CHECK(draw(g,b,0,255,0)); CHECK(draw(g,a,255,0,0)); }
            else { CHECK(draw(g,a,255,0,0)); CHECK(draw(g,b,0,255,0)); }
            CHECK(read_pixels(r,pixels));
            if (!reverse) memcpy(first,pixels,sizeof first);
            else CHECK(memcmp(first,pixels,sizeof first)==0);
        }
        sb_graphics_clear(g,0,0,0);
        sb_graphics_rect(g,10,10,20,10,255,255,255,true); CHECK(read_pixels(r,pixels));
        for (int i=0;i<W*H;++i) CHECK((first[i*4] || first[i*4+1]) == (pixels[i*4]!=0));
    }
    /* Full-range geometry: boundary x=y. Two halves exactly cover the screen.
     * The descending edge owns ties, so upper-right triangle includes x=y. */
    const int a[6]={INT_MIN,INT_MIN,INT_MAX,INT_MIN,INT_MAX,INT_MAX};
    const int b[6]={INT_MIN,INT_MIN,INT_MAX,INT_MAX,INT_MIN,INT_MAX};
    for (int reverse=0;reverse<2;++reverse) {
        sb_graphics_clear(g,0,0,0);
        if (reverse) { CHECK(draw(g,b,0,255,0)); CHECK(draw(g,a,255,0,0)); }
        else { CHECK(draw(g,a,255,0,0)); CHECK(draw(g,b,0,255,0)); }
        CHECK(read_pixels(r,pixels));
        for (int y=0;y<H;++y) for (int x=0;x<W;++x) {
            int i=(y*W+x)*4;
            CHECK(pixels[i]==(x>=y?255:0) && pixels[i+1]==(x<y?255:0));
        }
    }
    /* Golden membership hashes from arbitrary-precision Python edge equations,
     * FNV-1a over row-major 0/1 coverage. Full-range non-diagonal edges exercise
     * quotient/remainder rounding beyond signed64 cross-product capacity. */
    const struct { int v[6]; uint64_t hash; } golden[] = {
        {{INT_MIN,INT_MIN,2147483647,2147483646,INT_MIN,2147483647}, UINT64_C(9880668095422224517)},
        {{INT_MIN,-2147483647,2147483647,2147483646,2147483647,INT_MIN}, UINT64_C(4308609055973157141)},
        {{INT_MIN,-1000000000,2147483647,1000000031,INT_MIN,2147483647}, UINT64_C(13553667213826984383)},
        {{INT_MIN,1000000031,2147483647,-1000000000,2147483647,2147483647}, UINT64_C(7290781129379254308)},
        {{INT_MIN,INT_MIN,2147483647,2147483647,2147483647,2147483646}, UINT64_C(2604353852830947157)},
        {{INT_MIN,INT_MIN,2147483647,2147483647,0,0}, UINT64_C(9119243507623858981)},
        {{INT_MIN,17,2147483647,18,32,2147483647}, UINT64_C(14715070099880659621)},
        {{31,INT_MIN,32,2147483647,2147483647,24}, UINT64_C(2871093660177750309)},
    };
    for (unsigned k=0;k<sizeof golden/sizeof golden[0];++k) {
        sb_graphics_clear(g,0,0,0);
        CHECK(draw(g,golden[k].v,255,255,255)); CHECK(read_pixels(r,pixels));
        uint64_t hash=UINT64_C(14695981039346656037);
        for (int i=0;i<W*H;++i) {
            hash ^= pixels[i*4]!=0;
            hash *= UINT64_C(1099511628211);
        }
        CHECK(hash==golden[k].hash);
    }
    const int cameras[]={INT_MIN,INT_MAX-100,-100,100};
    const int small[6]={3,5,55,11,14,43};
    for (unsigned k=0;k<sizeof cameras/sizeof cameras[0];++k) {
        int c=cameras[k], v[6];
        for (int i=0;i<6;++i) v[i]=small[i]+c;
        sb_graphics_camera(g,c,c); sb_graphics_clear(g,0,0,0);
        CHECK(draw(g,v,255,255,255)); CHECK(read_pixels(r,pixels));
        for (int y=0;y<H;++y) for (int x=0;x<W;++x)
            CHECK(pixels[(y*W+x)*4]==(oracle(small,x,y)?255:0));
    }
    sb_graphics_camera(g,INT_MIN,INT_MAX); sb_graphics_clear(g,0,0,0);
    CHECK(draw(g,a,255,255,255)); CHECK(read_pixels(r,pixels));
    for (int i=0;i<W*H;++i) CHECK(pixels[i*4]==0);
    sb_graphics_camera_reset(g);
    CHECK(sb_graphics_clip(g,0,0,0,0));
    CHECK(draw(g,a,255,255,255)); CHECK(read_pixels(r,pixels));
    for (int i=0;i<W*H;++i) CHECK(pixels[i*4]==0);
    sb_graphics_destroy(g); SDL_Quit(); return 0;
}
