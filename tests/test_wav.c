#include "../src/sb_wav_internal.h"
#include "wav_fixture.h"
#include <math.h>
#include <stdio.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"WAV line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static bool fail_malloc;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) { return fail_malloc ? NULL : __real_malloc(size); }
static bool rejects(const void *b,size_t n) {
    sb_wav_data_t d={0}; sb_wav_result_t r=sb_wav_decode(b,n,&d);
    bool ok=r!=SB_WAV_OK && !d.samples && !d.frames && !d.channels;
    sb_wav_free(&d); return ok;
}
int main(void) {
    size_t n; sb_wav_data_t d={0};
    unsigned rates[]={22050,44100,48000};
    for (unsigned c=1;c<=2;++c) for (unsigned r=0;r<3;++r) {
        unsigned char *b=fixture(rates[r],c,7,&n); CHECK(b);
        put16(b+44,32768); put16(b+44+c*2,0);
        CHECK(sb_wav_decode(b,n,&d)==SB_WAV_OK);
        CHECK(d.channels==c && d.frames==(7u*48000+rates[r]-1)/rates[r]);
        CHECK(d.samples[0]==-1);
        for (size_t i=0;i<d.frames*c;++i) CHECK(d.samples[i]>=-1 && d.samples[i]<1);
        double expected=-1+(double)rates[r]/48000;
        CHECK(fabs(d.samples[c]-expected)<1e-6);
        CHECK(d.samples[(d.frames-1)*c]==32767.0f/32768);
        sb_wav_free(&d); free(b);
    }
    unsigned char *b=fixture(48000,1,4,&n); CHECK(b);
    for (size_t len=0;len<n;++len) CHECK(rejects(b,len));
    CHECK(rejects(NULL,n)); CHECK(rejects(b,SB_WAV_SOURCE_LIMIT+1u));
    unsigned char saved[52]; memcpy(saved,b,n);
    const unsigned offsets[]={0,8,16,20,22,24,28,32,34,40};
    for (unsigned i=0;i<sizeof offsets/sizeof *offsets;++i) {
        b[offsets[i]]=255; CHECK(rejects(b,n)); memcpy(b,saved,n);
    }
    fail_malloc=true; CHECK(sb_wav_decode(b,n,&d)==SB_WAV_NO_MEMORY); fail_malloc=false;
    /* Unknown odd-sized chunk and required pad, duplicate fmt/data rejection. */
    unsigned char odd[62]; memcpy(odd,b,12); memcpy(odd+12,"JUNK",4);
    put32(odd+16,1); odd[20]=42; odd[21]=0; memcpy(odd+22,b+12,n-12);
    put32(odd+4,sizeof odd-8); CHECK(sb_wav_decode(odd,sizeof odd,&d)==SB_WAV_OK); sb_wav_free(&d);
    unsigned char duplicate[76]; memcpy(duplicate,b,n); memcpy(duplicate+n,b+12,24);
    put32(duplicate+4,sizeof duplicate-8); CHECK(rejects(duplicate,sizeof duplicate));
    memcpy(duplicate+n,b+36,16); put32(duplicate+4,68-8); CHECK(rejects(duplicate,68));
    unsigned char ext[54]; memcpy(ext,b,36); put32(ext+16,18); put16(ext+36,0);
    memcpy(ext+38,b+36,16); put32(ext+4,sizeof ext-8);
    CHECK(sb_wav_decode(ext,sizeof ext,&d)==SB_WAV_OK); sb_wav_free(&d);
    ext[36]=1; CHECK(rejects(ext,sizeof ext));
    /* Mutated headers/data remain bounded, and every successful decode frees. */
    uint32_t random=17;
    for (unsigned i=0;i<5000;++i) {
        memcpy(b,saved,n); random=random*1664525u+1013904223u;
        b[random%n]^=(unsigned char)(random>>24);
        sb_wav_result_t result=sb_wav_decode(b,n,&d);
        if (result==SB_WAV_OK) CHECK(d.samples && d.frames && d.channels<=2);
        sb_wav_free(&d);
    }
    free(b);
    b=fixture(48000,1,SB_WAV_ASSET_LIMIT/sizeof(float)+1,&n); CHECK(b);
    CHECK(sb_wav_decode(b,n,&d)==SB_WAV_TOO_LARGE); free(b);
    /* Exact decoded budget: eight 4 MiB assets; ninth remains caller-owned. */
    b=fixture(48000,1,SB_WAV_ASSET_LIMIT/sizeof(float),&n); CHECK(b);
    sb_wav_pool_t pool={0}; sb_wav_handle_t asset=0;
    for (unsigned i=0;i<8;++i) {
        CHECK(sb_wav_decode(b,n,&d)==SB_WAV_OK);
        CHECK(sb_wav_insert(&pool,&d,&asset)==SB_WAV_OK && !d.samples);
    }
    CHECK(pool.bytes==SB_WAV_TOTAL_LIMIT);
    CHECK(sb_wav_decode(b,n,&d)==SB_WAV_OK);
    CHECK(sb_wav_insert(&pool,&d,&asset)==SB_WAV_BUDGET && d.samples && asset==0);
    sb_wav_free(&d); sb_wav_pool_reset(&pool); CHECK(pool.bytes==0); free(b);
    b=fixture(48000,2,1024,&n); CHECK(b);
    for (unsigned i=0;i<64;++i) {
        CHECK(sb_wav_decode(b,n,&d)==SB_WAV_OK);
        CHECK(sb_wav_insert(&pool,&d,&asset)==SB_WAV_OK);
    }
    CHECK(sb_wav_decode(b,n,&d)==SB_WAV_OK);
    CHECK(sb_wav_insert(&pool,&d,&asset)==SB_WAV_ASSETS_FULL); sb_wav_free(&d);
    asset=pool.assets[0].id;
    sb_wav_handle_t ids[8],id;
    for (unsigned i=0;i<8;++i) CHECK(sb_wav_play(&pool,asset,1,0,&ids[i])==SB_WAV_OK);
    CHECK(sb_wav_play(&pool,asset,1,0,&id)==SB_WAV_VOICES_FULL && id==0);
    CHECK(sb_wav_unload(&pool,asset)==SB_WAV_IN_USE);
    float out[2048]; sb_wav_render(&pool,out,1);
    CHECK(fabs(out[0]-.2*32767/32768)<1e-7 && out[0]==out[1]);
    CHECK(sb_wav_stop(&pool,ids[0])); CHECK(!sb_wav_stop(&pool,ids[0]));
    CHECK(!sb_wav_controls(&pool,ids[0],1,0));
    CHECK(!sb_wav_controls(&pool,ids[1],NAN,0));
    sb_wav_render(&pool,out,479); CHECK(sb_wav_playing(&pool,ids[0]));
    sb_wav_render(&pool,out,1); CHECK(!sb_wav_playing(&pool,ids[0]));
    CHECK(sb_wav_play(&pool,asset,.5,-1,&id)==SB_WAV_OK && id!=ids[0]);
    CHECK(!sb_wav_stop(&pool,ids[0]));
    sb_wav_render(&pool,out,1024);
    for (unsigned i=0;i<8;++i) CHECK(!sb_wav_playing(&pool,ids[i]));
    CHECK(!sb_wav_playing(&pool,id)); CHECK(sb_wav_unload(&pool,asset)==SB_WAV_OK);
    CHECK(sb_wav_unload(&pool,asset)==SB_WAV_BAD_HANDLE);
    sb_wav_pool_reset(&pool);
    pool.serial=UINT64_MAX;
    CHECK(sb_wav_decode(b,n,&d)==SB_WAV_OK);
    CHECK(sb_wav_insert(&pool,&d,&asset)==SB_WAV_IDS_EXHAUSTED); sb_wav_free(&d);
    free(b); sb_wav_pool_reset(&pool);
    puts("WAV decode, limits, handles, and playback checks passed");
    return 0;
}
