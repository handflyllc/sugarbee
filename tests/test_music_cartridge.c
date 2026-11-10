#define _POSIX_C_SOURCE 200809L
#include "../src/sb_audio_sdl.c"
#include "music_asset_fixture.h"
#include "vcart.h"
#include <minizip/zip.h>
#include <minizip/unzip.h>
#include <unistd.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Music cartridge line %d: %s\n",__LINE__,#e); return 1; } } while (0)
/* Observe entry opening and application allocations, not minizip internals. */
int __real_unzOpenCurrentFile(unzFile);
void *__real_malloc(size_t);
void __real_free(void *);
static unsigned opened;
static size_t allocations,frees,fail_call,largest;
int __wrap_unzOpenCurrentFile(unzFile f) { ++opened; return __real_unzOpenCurrentFile(f); }
void *__wrap_malloc(size_t n) {
    if (n>largest) largest=n;
    if (++allocations==fail_call) return NULL;
    return __real_malloc(n);
}
void __wrap_free(void *p) { if (p) ++frees; __real_free(p); }
static char archive[512],payload[512],json[8192];
static bool make_cart(const char *name,const char *data,size_t length,bool duplicate,bool damage) {
    zipFile z=zipOpen(payload,APPEND_STATUS_CREATE); if (!z) return false;
    bool ok=true;
    for (unsigned i=0;i<(duplicate?2u:1u);++i) {
        if (zipOpenNewFileInZip(z,name,NULL,NULL,0,NULL,0,NULL,0,0)!=ZIP_OK) { ok=false; break; }
        if (zipWriteInFileInZip(z,data,(unsigned)length)!=ZIP_OK) ok=false;
        if (zipCloseFileInZip(z)!=ZIP_OK) ok=false;
    }
    if (zipClose(z,NULL)!=ZIP_OK) ok=false;
    if (!ok) return false;
    FILE *in=fopen(payload,"rb"),*out=fopen(archive,"wb");
    if (!in || !out) { if (in) fclose(in); if (out) fclose(out); return false; }
    if (fseek(in,0,SEEK_END)!=0) ok=false;
    long size=ftell(in); if (size<0) ok=false;
    rewind(in);
    vcart_header_t h={0}; memcpy(h.magic,VCART_MAGIC,4);
    h.header_version=VCART_STRUCT_VERSION; h.min_os_major=VCART_OS_MAJOR;
    h.cart_uuid[6]=0x40; h.cart_uuid[8]=0x80; h.payload_size=(uint32_t)size;
    memcpy(h.title,"Music boundary fixture",23);
    if (vcart_write_header(out,&h)!=VCART_OK) ok=false;
    unsigned char buffer[4096]; size_t n;
    while ((n=fread(buffer,1,sizeof buffer,in))) if (fwrite(buffer,1,n,out)!=n) ok=false;
    if (ferror(in)) ok=false;
    if (fclose(in)!=0) ok=false;
    if (fclose(out)!=0) ok=false;
    if (damage) {
        /* First stored member starts after its 30-byte local header/name. */
        out=fopen(archive,"r+b"); if (!out) return false;
        if (fseek(out,(long)(VCART_HEADER_SIZE+30+strlen(name)),SEEK_SET)!=0 ||
            fputc('!',out)==EOF) ok=false;
        if (fclose(out)!=0) ok=false;
    }
    return ok;
}
static float out[2048];
static bool render(sb_audio_t *a,unsigned frames) {
    size_t m=allocations,f=frees;
    audio_output(a,(Uint8 *)out,(int)(frames*2*sizeof(float)));
    return m==allocations && f==frees;
}
int main(void) {
    char dir[]="/tmp/sugarbee-music-cart-XXXXXX"; CHECK(mkdtemp(dir));
    CHECK(snprintf(archive,sizeof archive,"%s/test.vcart",dir)>0);
    CHECK(snprintf(payload,sizeof payload,"%s/payload.zip",dir)>0);
    size_t length=music_json(json,sizeof json,2); CHECK(length);
    CHECK(make_cart("assets/song.json",json,length,false,false));
    sb_audio_t *a=sb_audio_create(); CHECK(a); SDL_PauseAudioDevice(a->device,1);
    sb_music_handle_t id=42,next; sb_asset_result_t read; sb_music_load_result_t detail;
    CHECK(sb_audio_music_load_cartridge(NULL,archive,"assets/song.json",&id,&read,&detail)==SB_MUSIC_ASSET_UNAVAILABLE && !id);
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",NULL,NULL,NULL)==SB_MUSIC_ASSET_ARGUMENT);
    opened=0;
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&id,NULL,NULL)==SB_MUSIC_ASSET_REJECTED && !opened);
    sb_audio_session_begin(a);
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&id,&read,&detail)==SB_MUSIC_ASSET_OK);
    CHECK(read==SB_ASSET_OK && detail.code==SB_MUSIC_LOAD_OK && id);
    CHECK(sb_audio_music_state(a)==SB_MUSIC_IDLE); /* Loading does not autoplay. */
    unsigned map[]={3};
    CHECK(sb_audio_music_play(a,id,map,1,true)==SB_MUSIC_ASSET_OK);
    CHECK(render(a,10));
    sb_music_shared_t state=a->music; sb_music_assets_t pool=a->music_assets;
    const char *bad_paths[]={"../song.json","/assets/song.json","assets\\song.json","assets//song.json","./assets/song.json","assets/../song.json"};
    for (unsigned i=0;i<sizeof bad_paths/sizeof *bad_paths;++i) {
        CHECK(sb_audio_music_load_cartridge(a,archive,bad_paths[i],&next,&read,&detail)==SB_MUSIC_ASSET_READ_FAILED);
        CHECK(read==SB_ASSET_INVALID_PATH && !next);
    }
    CHECK(sb_audio_music_load_cartridge(a,archive,"missing.json",&next,&read,&detail)==SB_MUSIC_ASSET_READ_FAILED && read==SB_ASSET_NOT_FOUND && !next);
    CHECK(sb_audio_music_load_cartridge(a,"/nonexistent/music.vcart","song.json",&next,&read,NULL)==SB_MUSIC_ASSET_READ_FAILED && read==SB_ASSET_IO_ERROR && !next);
    CHECK(make_cart("assets/song.json",json,length,true,false));
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,NULL)==SB_MUSIC_ASSET_READ_FAILED && read==SB_ASSET_CORRUPT);
    CHECK(make_cart("../song.json",json,length,false,false));
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,NULL)==SB_MUSIC_ASSET_READ_FAILED && read==SB_ASSET_CORRUPT);
    CHECK(make_cart("assets/song.json",json,length,false,true));
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,NULL)==SB_MUSIC_ASSET_READ_FAILED && read==SB_ASSET_CORRUPT);
    CHECK(make_cart("assets/song.json","{}",2,false,false));
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,&detail)==SB_MUSIC_ASSET_LOAD_FAILED && !next);
    CHECK(read==SB_ASSET_OK && detail.code==SB_MUSIC_LOAD_SCHEMA);
    CHECK(make_cart("assets/song.json","{",1,false,false));
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,&detail)==SB_MUSIC_ASSET_LOAD_FAILED && detail.code==SB_MUSIC_LOAD_SYNTAX);
    CHECK(make_cart("assets/song.json","",0,false,false));
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,&detail)==SB_MUSIC_ASSET_LOAD_FAILED && !next);
    CHECK(!memcmp(&state,&a->music,sizeof state) && !memcmp(&pool,&a->music_assets,sizeof pool));
    /* Boundary before opening/decompressing the target member. */
    char *big=malloc(SB_MUSIC_LOAD_MAX_INPUT+1); CHECK(big);
    memset(big,' ',SB_MUSIC_LOAD_MAX_INPUT+1); memcpy(big,json,length);
    CHECK(make_cart("assets/song.json",big,SB_MUSIC_LOAD_MAX_INPUT+1,false,false));
    opened=0; largest=0;
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,&detail)==SB_MUSIC_ASSET_READ_FAILED && read==SB_ASSET_TOO_LARGE && !next);
    CHECK(!opened && largest<SB_MUSIC_LOAD_MAX_INPUT);
    /* Legacy reader still accepts this smaller-than-16-MiB entry. */
    uint8_t *bytes=NULL; size_t size=0;
    CHECK(sb_asset_read(archive,"assets/song.json",&bytes,&size)==SB_ASSET_OK && size==SB_MUSIC_LOAD_MAX_INPUT+1); free(bytes);
    CHECK(make_cart("assets/song.json",big,SB_MUSIC_LOAD_MAX_INPUT,false,false)); free(big);
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,&detail)==SB_MUSIC_ASSET_OK);
    CHECK(sb_audio_music_unload(a,next)==SB_MUSIC_ASSET_OK);
    CHECK(make_cart("assets/song.json",json,length,false,false));
    /* Fault every application allocation through read + parse + prepare. */
    size_t initial=allocations;
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,&detail)==SB_MUSIC_ASSET_OK);
    size_t steps=allocations-initial;
    CHECK(sb_audio_music_unload(a,next)==SB_MUSIC_ASSET_OK); pool=a->music_assets;
    for (size_t i=1;i<=steps;++i) {
        fail_call=allocations+i; size_t before_m=allocations,before_f=frees;
        sb_music_asset_code_t code=sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,&detail);
        CHECK(code==SB_MUSIC_ASSET_READ_FAILED || code==SB_MUSIC_ASSET_LOAD_FAILED);
        CHECK(!next && allocations-before_m-1==frees-before_f);
        CHECK(!memcmp(&state,&a->music,sizeof state) && !memcmp(&pool,&a->music_assets,sizeof pool));
    }
    fail_call=0;
    for (unsigned i=1;i<8;++i) CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,NULL,NULL)==SB_MUSIC_ASSET_OK);
    pool=a->music_assets;
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,&detail)==SB_MUSIC_ASSET_CAPACITY && !next);
    CHECK(!memcmp(&pool,&a->music_assets,sizeof pool) && !memcmp(&state,&a->music,sizeof state));
    CHECK(sb_audio_music_unload(a,id)==SB_MUSIC_ASSET_IN_USE);
    /* Archive lifetime is independent of loaded playback. */
    CHECK(unlink(archive)==0); CHECK(render(a,1000));
    CHECK(sb_audio_music_state(a)==SB_MUSIC_PLAYING);
    sb_audio_session_fail(a); CHECK(render(a,480)); opened=0;
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,&read,NULL)==SB_MUSIC_ASSET_REJECTED && !opened);
    sb_audio_session_end(a); CHECK(!a->music_assets.bytes);
    sb_audio_session_begin(a); CHECK(make_cart("assets/song.json",json,length,false,false));
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&next,NULL,NULL)==SB_MUSIC_ASSET_OK && next>id);
    CHECK(sb_audio_music_play(a,id,map,1,true)==SB_MUSIC_ASSET_BAD_HANDLE);
    /* Header/truncation refusal is distinct from schema errors. */
    FILE *f=fopen(archive,"wb"); CHECK(f); CHECK(fputs("bad",f)>=0); CHECK(fclose(f)==0);
    CHECK(sb_audio_music_load_cartridge(a,archive,"assets/song.json",&id,&read,NULL)==SB_MUSIC_ASSET_READ_FAILED && read==SB_ASSET_INVALID_CARTRIDGE);
    sb_audio_destroy(a);
    CHECK(unlink(archive)==0 && unlink(payload)==0 && rmdir(dir)==0);
    puts("Cartridge music loading checks passed");
    return 0;
}
