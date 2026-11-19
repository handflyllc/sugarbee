/* Exercise the module loader with real host libraries and instruction hooks. */
#define main sugarbee_host_main
#include "../src/main.c"
#undef main
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int run(lua_State *L,const char *code) {
    int status=luaL_dostring(L,code);
    if (status!=LUA_OK) fprintf(stderr,"%s\n",lua_tostring(L,-1));
    return status==LUA_OK;
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
    for (size_t i=64;ok && i+46+12<=(size_t)size;++i) {
        if (memcmp(data+i,"PK\001\002",4)==0 &&
            data[i+28]==12 && data[i+29]==0 &&
            memcmp(data+i+46,"large/m1.lua",12)==0) {
            data[i+16]^=1; found=1; break;
        }
    }
    /* Match the local header's deliberately wrong CRC, so minizip accepts
     * the header and detects corruption only after decompression. */
    for (size_t i=64;ok && i+30+12<=(size_t)size;++i) {
        if (memcmp(data+i,"PK\003\004",4)==0 &&
            data[i+26]==12 && data[i+27]==0 &&
            memcmp(data+i+30,"large/m1.lua",12)==0) {
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
    char corrupt[4096];
    const char *cart=argv[1];
    if (!strcmp(argv[2],"crc")) {
        CHECK(snprintf(corrupt,sizeof corrupt,"%s.corrupt",cart)<(int)sizeof corrupt);
        CHECK(corrupt_crc(cart,corrupt));
        cart=corrupt;
    }
    for (int session=0;session<2;++session) {
        lua_State *L=luaL_newstate(); CHECK(L);
        g_engine.L=L; g_engine.guru_active=false;
        open_cartridge_libraries(L);
        sb_lua_modules_register(L,cart);
        CHECK(lua_gettop(L)==0);
        const char *mode=argv[2];
        if (!strcmp(mode,"basic")) {
            CHECK(run(L,
                "assert(package==nil and io==nil and os==nil); "
                "assert(require('empty')==true and require(string.rep('a',235))==5); "
                "local a=require('value'); assert(a.answer==42 and a.runs==1); "
                "assert(rawequal(a,require('nested.child'))); a.changed=8; "
                "assert(require('value').changed==8 and runs==1); "
                "assert(require('nothing')==true and require('nothing')==true and nil_runs==1); "
                "assert(require('falsevalue')==false and require('falsevalue')==false and false_runs==1); "
                "assert(select('#',require('scalar'))==1 and require('scalar')==17); "
                "assert(require('callable')()==23)"));
        } else if (!strcmp(mode,"errors")) {
            CHECK(run(L,
                "for _,v in ipairs({'','../x','x/y','x'..string.char(92)..'y','.x','x.','x..y','x-y','1x','/x','x'..string.char(0)..'y',string.rep('a',236)}) do "
                "assert(not pcall(require,v)) end; "
                "assert(not pcall(require)); assert(not pcall(require,12)); assert(not pcall(require,'value',1)); "
                "local ok,e=pcall(require,'absent'); assert(not ok and e:find('absent') and e:find('not found')); "
                "ok,e=pcall(require,'syntax'); assert(not ok and e:find('syntax.lua')); "
                "ok,e=pcall(require,'a'); assert(not ok and e:find('a -> b -> a',1,true)); "
                "break_cycle=true; assert(require('a')==9); "
                "ok,e=pcall(require,'retry'); assert(not ok and e:find('first attempt')); "
                "assert(runs==1); assert(require('retry')==require('value') and attempts==2 and runs==1); "
                "error_object={}; ok,e=pcall(require,'objecterror'); assert(not ok and e==error_object); "
                "ok,e=pcall(require,'binary'); assert(not ok and e:find('binary')); "
                "assert(not pcall(require,'oversize')); assert(require('scalar')==17)"));

        } else if (!strcmp(mode,"depth")) {
            CHECK(run(L,
                "local ok,e=pcall(require,'depth.m1'); assert(not ok and e:find('limit is 16')); "
                "assert(require('depth.m2')==17); "
                "assert(require('depth.m1')==17)"));
        } else if (!strcmp(mode,"count")) {
            CHECK(run(L,
                "for i=1,127 do assert(require('count.m'..i)==i) end; "
                "local ok,e=pcall(require,'parent'); assert(not ok and e:find('limit is 128')); "
                "assert(require('count.m128')==128); "
                "ok,e=pcall(require,'count.m129'); assert(not ok and e:find('limit is 128')); "
                "assert(require('count.m1')==1)"));
        } else if (!strcmp(mode,"bytes")) {
            CHECK(run(L,
                "for i=1,32 do assert(require('large.m'..i)==1) end; "
                "local ok,e=pcall(require,'large.m33'); assert(not ok and e:find('session remaining 0')); "
                "assert(require('large.m1')==1)"));
        } else if (!strcmp(mode,"failed_bytes")) {
            CHECK(run(L,
                "for i=1,32 do local ok,e=pcall(require,'badlarge'); assert(not ok and not e:find('source/archive limit')) end; "
                "local ok,e=pcall(require,'badlarge'); assert(not ok and e:find('session remaining 0'))"));
        } else if (!strcmp(mode,"crc")) {
            CHECK(run(L,
                "for i=1,32 do local ok,e=pcall(require,'large.m1'); "
                "assert(not ok and e:find('damaged ZIP'), tostring(e)) end; "
                "local ok,e=pcall(require,'large.m1'); assert(not ok and e:find('session remaining 0'), tostring(e))"));
        } else if (!strcmp(mode,"host")) {
            CHECK(load_virtual_cartridge(argv[1]));
            call_lua_global_function("init",false,0);
            CHECK(!g_engine.guru_active && run(L,"assert(main_loaded and initialized and runs==1)"));
            g_engine.cart_loaded=false;
        } else if (!strcmp(mode,"budget")) {
            CHECK(run(L,
                "function test() local ok=pcall(require,'runaway'); assert(not ok); end"));
            lua_getglobal(L,"test");
            CHECK(call_lua_with_limit(0,"module budget",10000,false)==LUA_OK);
            CHECK(g_engine.execution_limit_exceeded);
            CHECK(run(L,"assert(require('value').runs==1)")); /* Markers unwound. */
            CHECK(run(L,"function test() require('workprobe'); end"));
            lua_getglobal(L,"test");
            CHECK(call_lua_with_limit(0,"single import budget",250000,false)==LUA_OK);
            CHECK(!g_engine.execution_limit_exceeded);
            /* Two individually affordable imports must share one callback budget.
             * Same source via separate names avoids cached work. */
            CHECK(run(L,"function test() require('work'); require('workcopy'); end"));
            lua_getglobal(L,"test");
            int status=call_lua_with_limit(0,"shared module budget",250000,false);
            CHECK(status!=LUA_OK && g_engine.execution_limit_exceeded);
            lua_pop(L,1);
        } else return 2;
        CHECK(lua_gettop(L)==0);
        lua_close(L); g_engine.L=NULL;
    }
    if (!strcmp(argv[2],"crc")) CHECK(remove(corrupt)==0);
    return 0;
}
