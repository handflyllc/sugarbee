#include "sb_console_config.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"Line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    int w,h; char error[256], source[128];
    const int sizes[][2]={{300,200},{320,240},{640,360},{960,720},{1280,720}};
    CHECK(sb_console_config_parse("{}",2,&w,&h,error,sizeof(error)) && w==1280 && h==720);
    for (size_t i=0;i<5;++i) {
        snprintf(source,sizeof(source),"{\"video\":{\"width\":%d,\"height\":%d}}",sizes[i][0],sizes[i][1]);
        CHECK(sb_console_config_parse(source,strlen(source),&w,&h,error,sizeof(error)));
        CHECK(w==sizes[i][0] && h==sizes[i][1] && !error[0]);
    }
    const char *bad[]={"", "null", "[]", "true", "{", "{}x", "{} {}", "{\"video\":null}",
        "{\"vide0\":{}}", "{\"video\":{}}", "{\"video\":{\"width\":300}}",
        "{\"video\":{\"width\":300,\"height\":200,\"scale\":3}}",
        "{\"video\":{\"width\":300,\"height\":200},\"video\":{\"width\":300,\"height\":200}}",
        "{\"video\":{\"width\":300,\"width\":300,\"height\":200}}",
        "{\"video\":{\"width\":300,\"height\":200,\"height\":200}}",
        "{\"video\":{\"width\":300.0,\"height\":200}}",
        "{\"video\":{\"width\":\"300\",\"height\":200}}",
        "{\"video\":{\"width\":true,\"height\":200}}",
        "{\"video\":{\"width\":-300,\"height\":200}}",
        "{\"video\":{\"width\":0,\"height\":200}}",
        "{\"video\":{\"width\":300,\"height\":240}}",
        "{\"video\":{\"width\":18446744073709551615,\"height\":200}}",
        "{\"video\":{\"width\":1e400,\"height\":200}}",
        "{\"video\\u0000\":{\"width\":300,\"height\":200}}",
        "{\"video\":{\"width\":300,\"height\":200,}}", "{/*comment*/}"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        CHECK(!sb_console_config_parse(bad[i],strlen(bad[i]),&w,&h,error,sizeof(error)));
        CHECK(w==1280 && h==720 && strstr(error,"console.json"));
    }
    char boundary[SB_CONSOLE_CONFIG_MAX+1]; memset(boundary,' ',sizeof(boundary));
    memcpy(boundary,"{}",2);
    CHECK(sb_console_config_parse(boundary,SB_CONSOLE_CONFIG_MAX,&w,&h,error,sizeof(error)));
    CHECK(!sb_console_config_parse(boundary,sizeof(boundary),&w,&h,error,sizeof(error)));
    CHECK(!sb_console_config_parse("{}\0junk",7,&w,&h,error,sizeof(error)));
    puts("console config checks pass"); return 0;
}
