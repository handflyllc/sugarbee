/* Production binding, counted archive reads, allocation cleanup and lifecycle. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int run(lua_State *L,const char *code) {
    int status=luaL_dostring(L,code);
    if(status!=LUA_OK) fprintf(stderr,"%s\n",lua_tostring(L,-1));
    return status==LUA_OK;
}
/* Native data buffer accounting and a deterministic Lua allocation failure
 * after reading the buffer. Linker wrappers are confined to this test. */
void *__real_malloc(size_t size);
void __real_free(void *ptr);
static void *read_buffer;
static bool fail_after_read, lua_fail;
static lua_Alloc upstream;
static void *upstream_ud;
void *__wrap_malloc(size_t size) {
    void *ptr=__real_malloc(size);
    if(ptr && size==SB_DATA_MAX_FILE+1) {
        if(read_buffer) abort();
        read_buffer=ptr;
        if(fail_after_read) lua_fail=true;
    }
    return ptr;
}
void __wrap_free(void *ptr) {
    if(ptr && ptr==read_buffer) read_buffer=NULL;
    __real_free(ptr);
}
static void *test_alloc(void *ud,void *ptr,size_t old,size_t size) {
    (void)ud;
    if(lua_fail && size>old) return NULL;
    return upstream(upstream_ud,ptr,old,size);
}
/* Corrupt only the CRC of a known member in a copy of the packed fixture.
 * Keeps valid source bytes/ZIP lengths so the failure occurs after reading. */
static int corrupt_crc(const char *input,const char *output) {
    FILE *f=fopen(input,"rb");
    if (!f) return 0;
    if (fseek(f,0,SEEK_END)) { fclose(f); return 0; }
    long size=ftell(f); rewind(f);
    if (size<64) { fclose(f); return 0; }
    unsigned char *data=malloc((size_t)size);
    if (!data) { fclose(f); return 0; }
    int ok=fread(data,1,(size_t)size,f)==(size_t)size;
    fclose(f);
    int found=0;
    for (size_t i=64;ok && i+46+9<=(size_t)size;++i) {
        if (memcmp(data+i,"PK\001\002",4)==0 &&
            data[i+28]==9 && data[i+29]==0 &&
            memcmp(data+i+46,"large.bin",9)==0) {
            data[i+16]^=1; found=1; break;
        }
    }
    /* Match the local header's deliberately wrong CRC, so minizip accepts
     * the header and detects corruption only after decompression. */
    for (size_t i=64;ok && i+30+9<=(size_t)size;++i) {
        if (memcmp(data+i,"PK\003\004",4)==0 &&
            data[i+26]==9 && data[i+27]==0 &&
            memcmp(data+i+30,"large.bin",9)==0) {
            data[i+14]^=1; break;
        }
    }
    if (ok && found) {
        f=fopen(output,"wb");
        if (!f) ok=0;
        else { ok=fwrite(data,1,(size_t)size,f)==(size_t)size; if (fclose(f)) ok=0; }
    }
    free(data); return ok && found;
}
int main(int argc,char **argv) {
    CHECK(argc==3);
    const char *mode=argv[2],*cart=argv[1]; char corrupt[4096];
    if(!strcmp(mode,"crc")) {
        CHECK(snprintf(corrupt,sizeof(corrupt),"%s.data-crc",cart)<(int)sizeof(corrupt));
        CHECK(corrupt_crc(cart,corrupt)); cart=corrupt;
    }
    if(!strcmp(mode,"invalid")) {
        CHECK(snprintf(corrupt,sizeof(corrupt),"%s.invalid",cart)<(int)sizeof(corrupt));
        cart=corrupt;
    }
    if(!strcmp(mode,"host")) {
        CHECK(init_engine());
        for(int session=0;session<2;++session) {
            CHECK(start_cartridge(cart));
            CHECK(run(g_engine.L,"assert(greeting=='Hello\\ncartridge!\\n'); assert(#top_bytes==6)"));
            call_lua_global_function("update",true,1.0/60);
            CHECK(sb_graphics_begin_game_frame(g_engine.graphics));
            call_lua_global_function("draw",false,0);
            CHECK(sb_graphics_show_game_frame(g_engine.graphics));
            CHECK(!g_engine.guru_active);
            set_cartridge_paused(true); set_cartridge_paused(false);
            end_cartridge(); CHECK(!g_engine.L && !g_engine.guru_active);
        }
        shutdown_engine(); return 0;
    }
    for(int session=0;session<2;++session) {
        lua_State *L=luaL_newstate(); CHECK(L);
        open_cartridge_libraries(L);
        lua_newtable(L);
        sb_lua_data_register(L, !strcmp(mode,"io") ? "/nonexistent/sugarbee-data.vcart" : cart);
        CHECK(lua_gettop(L)==1); lua_setglobal(L,"sugarbee");
        CHECK(run(L,"read=sugarbee.read_file"));
        if(!strcmp(mode,"basic")) {
            CHECK(run(L,"local a,e=read('assets/bytes.bin'); assert(a==string.char(0,1,255,10,0,13) and e==nil); "
                "assert(select('#',read('assets/bytes.bin'))==2); assert(read('assets/text.txt')=='Hello\\ncartridge!\\n'); "
                "local z,e=read('empty'); assert(z=='' and e==nil); assert(read(string.rep('a',239))=='long path'); "
                "assert(io==nil and os==nil); assert(type(read('main.lua'))=='string'); "
                "local s=read('one'); collectgarbage(); assert(s=='q')"));
        } else if(!strcmp(mode,"errors")) {
            CHECK(run(L,"assert(not pcall(read)); assert(not pcall(read,1)); assert(not pcall(read,false)); "
                "assert(not pcall(read,{})); assert(not pcall(read,'one',nil)); "
                "for _,p in ipairs({'','/etc/passwd','../one','a/../one','a//b','a/./b','a/','a:b','a'..string.char(92)..'b', "
                "'one'..string.char(0)..'suffix','a'..string.char(10),string.rep('a',240)}) do assert(not pcall(read,p)) end; "
                "local a,e=read('absent'); assert(a==nil and e=='not_found' and select('#',read('absent'))==2); "
                "a,e=read('oversized'); assert(a==nil and e=='too_large'); "
                "for i=1,16 do assert(#read('large.bin')==1048576) end; "
                "a,e=read('one'); assert(a==nil and e=='too_large')"));
        } else if(!strcmp(mode,"budget")) {
            CHECK(run(L,"local retained=assert(read('large.bin')); for i=2,16 do assert(#read('large.bin')==1048576) end; "
                "collectgarbage(); local a,e=read('large.bin'); assert(a==nil and e=='too_large'); "
                "assert(#retained==1048576 and retained:sub(-1)=='x'); "
                "assert(read('empty')==''); a,e=read('absent'); assert(a==nil and e=='not_found')"));
        } else if(!strcmp(mode,"partial")) {
            CHECK(run(L,"for i=1,15 do assert(#read('large.bin')==1048576) end; "
                "assert(read('one')=='q'); local a,e=read('large.bin'); assert(a==nil and e=='too_large'); "
                "assert(#read('almost')==1048575); a,e=read('one'); assert(a==nil and e=='too_large')"));
        } else if(!strcmp(mode,"crc")) {
            CHECK(run(L,"for i=1,16 do local a,e=read('large.bin'); assert(a==nil and e=='corrupt',tostring(e)) end; "
                "local a,e=read('large.bin'); assert(a==nil and e=='too_large'); assert(read('empty')=='')"));
        } else if(!strcmp(mode,"oom")) {
            upstream=lua_getallocf(L,&upstream_ud); lua_setallocf(L,test_alloc,NULL);
            lua_getglobal(L,"read"); lua_pushliteral(L,"large.bin");
            fail_after_read=true;
            int status=lua_pcall(L,1,2,0);
            fail_after_read=false; lua_fail=false;
            CHECK(status==LUA_ERRMEM && !read_buffer);
            lua_pop(L,1);
            CHECK(run(L,"for i=1,15 do assert(#read('large.bin')==1048576) end; "
                "local a,e=read('large.bin'); assert(a==nil and e=='too_large')"));
            lua_setallocf(L,upstream,upstream_ud);
        } else if(!strcmp(mode,"invalid")) {
            CHECK(run(L,"local a,e=read('one'); assert(a==nil and e=='invalid_cartridge')"));
        } else if(!strcmp(mode,"io")) {
            CHECK(run(L,"local a,e=read('one'); assert(a==nil and e=='io_error')"));
        } else return 2;
        CHECK(lua_gettop(L)==0 && !read_buffer);
        lua_close(L);
    }
    puts("data reader checks pass"); return 0;
}
