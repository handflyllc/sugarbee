#include "../src/sb_music_load.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Load line %d: %s\n",__LINE__,#e); return 1; } } while (0)
/* Link wrapping affects the loader and vendored reader without shipping test
 * hooks in the native interface. Only track calls during each load/free. */
void *__real_malloc(size_t);
void __real_free(void *);
static bool tracing;
static size_t calls, live, fail_call;
void *__wrap_malloc(size_t n) {
    if (tracing && ++calls == fail_call) return NULL;
    void *p = __real_malloc(n);
    if (tracing && p) ++live;
    return p;
}
void __wrap_free(void *p) {
    if (tracing && p) --live;
    __real_free(p);
}
static char input[SB_MUSIC_LOAD_MAX_INPUT + 2], edited[SB_MUSIC_LOAD_MAX_INPUT + 2];
static float actual[8192], expected[8192];
static bool fixture(const char *name) {
    char path[512]; snprintf(path,sizeof(path),"%s/%s",MUSIC_FIXTURES,name);
    FILE *f = fopen(path,"rb"); if (!f) return false;
    size_t n = fread(input,1,sizeof(input)-1,f); bool ok = !ferror(f) && feof(f);
    fclose(f); input[n] = 0; return ok;
}
static bool replace(const char *old, const char *newtext) {
    const char *p = strstr(input,old); if (!p) return false;
    size_t a = (size_t)(p-input), b = strlen(newtext), c = strlen(p+strlen(old));
    if (a+b+c >= sizeof(edited)) return false;
    memcpy(edited,input,a); memcpy(edited+a,newtext,b); memcpy(edited+a+b,p+strlen(old),c+1);
    return true;
}
static int rejected(const char *s, size_t n) {
    sb_music_load_result_t r;
    tracing = true; live = calls = fail_call = 0;
    sb_music_asset_t *a = sb_music_load(s,n,&r);
    if (a) sb_music_asset_free(a);
    tracing = false;
    CHECK(!a && r.code != SB_MUSIC_LOAD_OK && r.message[0] && !live);
    return 0;
}
int main(void) {
    sb_music_load_result_t r;
    CHECK(fixture("chord.json"));
    sb_music_asset_t *a = sb_music_load(input,strlen(input),&r);
    CHECK(a && r.code == SB_MUSIC_LOAD_OK);
    printf("Chord: parser peak %zu, prepared %zu bytes\n",r.parser_peak_bytes,r.prepared_bytes);
    const sb_music_song_t *song = sb_music_asset_song(a);
    CHECK(song && song->track_count == 3 && song->event_count == 8);
    /* Independently written native reference, identical to timing test fixture. */
    sb_music_instrument_t ins[3] = {
        {SB_AUDIO_VOICE_TRIANGLE,{0,0,1,.002},.5,0,0,2000,true},
        {SB_AUDIO_VOICE_SAW,{0,0,1,0},.5,0,0,700,false},
        {SB_AUDIO_VOICE_PULSE,{0,0,1,0},.5,0,.5,2000,true}};
    sb_music_event_t ev[] = {
        {17,1,SB_MUSIC_NOTE_ON,220},{17,2,SB_MUSIC_NOTE_ON,275},{17,3,SB_MUSIC_NOTE_ON,330},
        {117,1,SB_MUSIC_NOTE_OFF,0},{117,2,SB_MUSIC_NOTE_OFF,0},{117,3,SB_MUSIC_NOTE_OFF,0},
        {117,1,SB_MUSIC_NOTE_ON,440},{317,1,SB_MUSIC_NOTE_OFF,0}};
    sb_music_song_t native = {48000,3,317,ins,ev,8};
    sb_music_sequence_t x = {0}, y = {0};
    memset(input,'X',sizeof(input)); /* no retained input or parser references */
    for (int replay=0;replay<2;++replay) {
        CHECK(sb_music_sequence_start(&x,song) && sb_music_sequence_start(&y,&native));
        CHECK(sb_music_sequence_render(&y,expected,4096));
        for (size_t n=0;n<4096;) {
            size_t block = n%71+1; if (block>4096-n) block=4096-n;
            CHECK(sb_music_sequence_render(&x,actual+2*n,block)); n+=block;
        }
        CHECK(memcmp(actual,expected,sizeof(actual))==0 && x.state==SB_MUSIC_IDLE);
    }
    sb_music_asset_free(a);
    CHECK(!sb_music_asset_song(NULL)); sb_music_asset_free(NULL);
    const char *valid[] = {"one-note.json","retrigger.json","noise.json","silent.json"};
    for (size_t j=0;j<4;++j) {
        CHECK(fixture(valid[j]));
        a=sb_music_load(input,strlen(input),&r); CHECK(a);
        CHECK(sb_music_sequence_start(&x,sb_music_asset_song(a)));
        CHECK(sb_music_sequence_render(&x,actual,4096));
        if (j==3) for(size_t k=0;k<8192;++k) CHECK(actual[k]==0);
        sb_music_sequence_reset(&x); sb_music_asset_free(a);
    }
    /* Noise is deterministic from the same initial bank state. */
    CHECK(fixture("noise.json"));
    a=sb_music_load(input,strlen(input),&r); CHECK(a);
    sb_music_instrument_t noise = {SB_AUDIO_VOICE_NOISE,{0,0,1,.002},.5,0,0,2000,true};
    sb_music_event_t noise_events[] = {{0,1,SB_MUSIC_NOTE_ON,0},{317,1,SB_MUSIC_NOTE_OFF,0}};
    sb_music_song_t noise_song = {48000,1,317,&noise,noise_events,2};
    CHECK(sb_music_sequence_start(&x,sb_music_asset_song(a)) && sb_music_sequence_start(&y,&noise_song));
    CHECK(sb_music_sequence_render(&x,actual,4096) && sb_music_sequence_render(&y,expected,4096));
    CHECK(!memcmp(actual,expected,sizeof(actual))); sb_music_asset_free(a);
    CHECK(fixture("retrigger.json"));
    CHECK(replace("\"frame\": 117", "\"frame\": 118")); CHECK(!rejected(edited,strlen(edited)));
    CHECK(replace("\"type\": \"note_off\"", "\"type\": \"note_on\", \"hz\": 440"));
    CHECK(!rejected(edited,strlen(edited)));
    CHECK(fixture("chord.json"));
    CHECK(replace("\"pulse_width\": 0.5", "\"pulse_width\": 0.049")); CHECK(!rejected(edited,strlen(edited)));
    CHECK(replace("\"pulse_width\": 0.5", "\"pulse_width\": 0.951")); CHECK(!rejected(edited,strlen(edited)));
    const char *widths[]={"\"pulse_width\": 0.05","\"pulse_width\": 0.95"};
    for(size_t j=0;j<2;++j) {
        CHECK(replace("\"pulse_width\": 0.5",widths[j]));
        a=sb_music_load(edited,strlen(edited),&r); CHECK(a); sb_music_asset_free(a);
    }
    CHECK(fixture("nine-unused-tracks.json"));
    a=sb_music_load(input,strlen(input),&r); CHECK(a);
    CHECK(sb_music_asset_song(a)->track_count==9);
    CHECK(sb_music_sequence_start(&x,sb_music_asset_song(a)));
    CHECK(sb_music_sequence_render(&x,actual,1) && x.state==SB_MUSIC_IDLE);
    sb_music_asset_free(a);
    CHECK(fixture("invalid-ten-tracks.json")); CHECK(!rejected(input,strlen(input)));
    CHECK(fixture("silent.json"));
    char *tracks=strstr(input,"\"tracks\": ["), *event_array=strstr(input,"\"events\": []");
    CHECK(tracks && event_array);
    size_t before=(size_t)(tracks-input);
    memcpy(edited,input,before);
    snprintf(edited+before,sizeof(edited)-before,"\"tracks\": [], %s",event_array);
    CHECK(!rejected(edited,strlen(edited)));
    CHECK(fixture("invalid-track.json")); CHECK(!rejected(input,strlen(input)));
    CHECK(fixture("invalid-duplicate.json")); CHECK(!rejected(input,strlen(input)));
    CHECK(fixture("one-note.json"));
    const char *changes[][2] = {
        {"\"version\": 1","\"version\": 2"}, {"\"version\": 1","\"version\": 1.0"},
        {"\"version\": 1","\"version\": 1e0"}, {"\"version\": 1","\"version\": true"},
        {"\"version\": 1","\"version\": \"1\""}, {"\"version\": 1","\"version\": 18446744073709551616"},
        {"sugarbee.music","sugarbee.other"}, {"48000","44100"},
        {"\"end_frame\": 317","\"end_frame\": 0"}, {"\"end_frame\": 317","\"end_frame\": 28800001"},
        {"\"end_frame\": 317","\"end_frame\": 316"}, {"\"frame\": 0","\"frame\": -0"},
        {"\"frame\": 0","\"frame\": 1.5"}, {"\"frame\": 0","\"frame\": 318"},
        {"\"track\": 1","\"track\": 0"}, {"\"track\": 1","\"track\": 2"},
        {"\"hz\": 440","\"hz\": 27.499999999999996"}, {"\"hz\": 440","\"hz\": 4186.010000000001"},
        {"\"hz\": 440","\"hz\": 1e999"}, {"\"hz\": 440","\"hz\": null"},
        {"\"hz\": 440","\"hz\": NaN"}, {"\"hz\": 440","\"hz\": 0440"},
        {"triangle","noise"}, {"triangle","pulse"}, {"triangle","other"},
        {"\"attack\": 0","\"attack\": -1"}, {"\"decay\": 0","\"decay\": 11"},
        {"\"sustain\": 1","\"sustain\": 1.01"}, {"\"release\": 0.002","\"release\": 11"},
        {"\"volume\": 0.5","\"volume\": 2"}, {"\"pan\": 0","\"pan\": -2"},
        {"\"cutoff\": 2000","\"cutoff\": 19"}, {"\"cutoff\": 2000","\"cutoff\": 20001"},
        {"\"bypass\": true","\"bypass\": 1"}, {"\"bypass\": true","\"bypass\": true, \"bypass\": false"},
        {"\"volume\": 0.5","\"volume\": 0.5, \"pulse_width\": 0.5"},
        {"\"attack\": 0,",""}, {"\"version\": 1,",""},
        {"\"version\": 1","\"version\": 1, \"extra\": 0"},
        {"\"version\": 1","\"version\": 1, \"\\u0076ersion\": 1"},
        {"\"version\": 1","\"version\\u0000\": 1"},
        {"note_on","note_off"}, {"note_off","note_on"},
        {"\"type\": \"note_off\"","\"type\": \"note_off\", \"hz\": 440"},
        {"\"frame\": 317","\"frame\": 0"},
        {"\"hz\": 440","\"hz\": 440, \"extra\": 0"},
        {"\"format\"","/* comment */ \"format\""},
        {"\"bypass\": true","\"bypass\": true,"},
        {"triangle","tri\xff"}, {"triangle","tri\\uD800"}
    };
    for (size_t j=0;j<sizeof(changes)/sizeof(*changes);++j) {
        CHECK(replace(changes[j][0],changes[j][1]));
        if (rejected(edited,strlen(edited))) { fprintf(stderr,"Mutation %zu\n",j); return 1; }
    }
    CHECK(!rejected(input,strlen(input)-3));
    CHECK(replace("{","[[[[[[")); CHECK(!rejected(edited,strlen(edited)));
    CHECK(replace("{","{} ")); CHECK(!rejected(edited,strlen(edited)));
    CHECK(!rejected("",0)); CHECK(!rejected(NULL,0));
    CHECK(!rejected("{}",SB_MUSIC_LOAD_MAX_INPUT+1)); /* length rejected before reading */
    /* NUL in input cannot hide trailing junk. */
    size_t len = strlen(input); memcpy(edited,input,len); edited[len]=0; edited[len+1]='x';
    CHECK(!rejected(edited,len+2));
    /* At most 5 containers; braces in strings don't consume depth budget. */
    CHECK(replace("triangle","{{{{{{"));
    a=sb_music_load(edited,strlen(edited),&r); CHECK(!a && r.code==SB_MUSIC_LOAD_SCHEMA);
    const char *boundaries[][2] = {
        {"\"hz\": 440","\"hz\": 27.5"}, {"\"hz\": 440","\"hz\": 4186.01"},
        {"\"hz\": 440","\"hz\": 4186.009044809578"},
        {"\"hz\": 440","\"hz\": 110"}, {"\"hz\": 440","\"hz\": 880"},
        {"\"attack\": 0","\"attack\": 10"}, {"\"decay\": 0","\"decay\": 10"},
        {"\"release\": 0.002","\"release\": 10"}, {"\"release\": 0.002","\"release\": 0"},
        {"\"volume\": 0.5","\"volume\": 0"}, {"\"volume\": 0.5","\"volume\": 1"},
        {"\"pan\": 0","\"pan\": -1"}, {"\"pan\": 0","\"pan\": 1"},
        {"\"cutoff\": 2000","\"cutoff\": 20"}, {"\"cutoff\": 2000","\"cutoff\": 20000"},
        {"\"hz\": 440","\"hz\": 4.4e2"}, {"\"version\": 1","\"\\u0076ersion\": 1"}
    };
    for(size_t j=0;j<sizeof(boundaries)/sizeof(*boundaries);++j) {
        CHECK(replace(boundaries[j][0],boundaries[j][1]));
        a=sb_music_load(edited,strlen(edited),&r); CHECK(a); sb_music_asset_free(a);
    }
    /* Maximum input size accepted when extra bytes are just legal whitespace. */
    len=strlen(input); memcpy(edited,input,len);
    memset(edited+len,' ',SB_MUSIC_LOAD_MAX_INPUT-len);
    a=sb_music_load(edited,SB_MUSIC_LOAD_MAX_INPUT,&r); CHECK(a); sb_music_asset_free(a);
    /* Every malloc site must unwind, including partially allocated parser state. */
    tracing=true; calls=live=fail_call=0;
    a=sb_music_load(input,strlen(input),&r); CHECK(a); size_t sites=calls;
    sb_music_asset_free(a); CHECK(live==0); tracing=false;
    for (size_t j=1;j<=sites;++j) {
        tracing=true; calls=live=0; fail_call=j;
        a=sb_music_load(input,strlen(input),&r);
        if(a) sb_music_asset_free(a);
        tracing=false;
        CHECK(!a && live==0 && r.code==SB_MUSIC_LOAD_MEMORY);
    }
    for(int j=0;j<100;++j) { a=sb_music_load(input,strlen(input),NULL); CHECK(a); sb_music_asset_free(a); }
    /* Generate maximum-event boundary and one over. */
    const char *events_pos=strstr(input,"\"events\": ["); CHECK(events_pos);
    size_t prefix=(size_t)(events_pos-input)+strlen("\"events\": [");
    memcpy(edited,input,prefix); size_t used=prefix;
    for(unsigned j=0;j<4096;++j) {
        int n=snprintf(edited+used,sizeof(edited)-used,
            "%s{\"frame\":%u,\"track\":1,\"type\":\"%s\"%s}",j?",":"",j,
            j%2?"note_off":"note_on",j%2?"":",\"hz\":440"); CHECK(n>0); used+=(size_t)n;
    }
    memcpy(edited+used,"]}",3);
    /* Grow the declared end to cover every generated event. */
    char *endpos=strstr(edited,"317"); CHECK(endpos);
    size_t offset=(size_t)(endpos-edited);
    memmove(edited+offset+4,edited+offset+3,strlen(edited+offset+3)+1);
    memcpy(edited+offset,"9999",4); ++used;
    a=sb_music_load(edited,strlen(edited),&r); CHECK(a);
    printf("4096 events: parser peak %zu, prepared %zu bytes\n",r.parser_peak_bytes,r.prepared_bytes);
    CHECK(r.parser_peak_bytes<=SB_MUSIC_LOAD_MAX_PARSER && r.prepared_bytes<=SB_MUSIC_LOAD_MAX_PREPARED);
    sb_music_asset_free(a);
    /* Force each allocation failure during large-parser buffer growth too. */
    tracing=true; calls=live=fail_call=0;
    a=sb_music_load(edited,strlen(edited),&r); CHECK(a); sites=calls;
    sb_music_asset_free(a); CHECK(!live); tracing=false;
    for(size_t j=1;j<=sites;++j) {
        tracing=true; calls=live=0; fail_call=j;
        a=sb_music_load(edited,strlen(edited),&r);
        if(a) sb_music_asset_free(a);
        tracing=false; CHECK(!a && !live && r.code==SB_MUSIC_LOAD_MEMORY);
    }
    strcpy(edited+used,",{\"frame\":4096,\"track\":1,\"type\":\"note_on\",\"hz\":440}]}");
    CHECK(!rejected(edited,strlen(edited)));
    /* Shallow, token-dense valid JSON exceeds parser budget before schema checks. */
    edited[0]='[';
    for(size_t j=1;j<SB_MUSIC_LOAD_MAX_INPUT-2;j+=2) { edited[j]='0'; edited[j+1]=','; }
    edited[SB_MUSIC_LOAD_MAX_INPUT-3]='0'; edited[SB_MUSIC_LOAD_MAX_INPUT-2]=']';
    CHECK(!rejected(edited,SB_MUSIC_LOAD_MAX_INPUT-1));
    a=sb_music_load(edited,SB_MUSIC_LOAD_MAX_INPUT-1,&r);
    CHECK(!a && r.code==SB_MUSIC_LOAD_LIMIT && r.parser_peak_bytes<=SB_MUSIC_LOAD_MAX_PARSER);
    puts("Music loader checks passed"); return 0;
}
