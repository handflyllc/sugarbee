#include "../src/sb_music_load.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Eligibility line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static char json[4096];
static float samples[2048];
static int check(double release, unsigned end, bool expected, bool retrigger,
                 bool muted, bool filter, bool silent, bool noise) {
    snprintf(json,sizeof(json),
        "{\"format\":\"sugarbee.music\",\"version\":1,\"timeline_rate\":48000,"
        "\"end_frame\":%u,\"tracks\":[{\"instrument\":{\"waveform\":\"%s\","
        "\"attack\":0.1,\"decay\":0,\"sustain\":%d,\"release\":%.17g,"
        "\"volume\":%d,\"pan\":0,\"filter\":{\"cutoff\":20,\"bypass\":%s}}}],\"events\":[%s%s%s]}",
        end,noise?"noise":"triangle",muted?0:1,release,muted?0:1,filter?"false":"true",
        silent?"":(noise?"{\"frame\":0,\"track\":1,\"type\":\"note_on\"},":"{\"frame\":0,\"track\":1,\"type\":\"note_on\",\"hz\":440},"),
        silent||!retrigger?"":"{\"frame\":1,\"track\":1,\"type\":\"note_off\"},{\"frame\":1,\"track\":1,\"type\":\"note_on\",\"hz\":220},",
        silent?"":"{\"frame\":4,\"track\":1,\"type\":\"note_off\"}");
    sb_music_load_result_t result;
    sb_music_asset_t *a=sb_music_load(json,strlen(json),&result);
    if(!a) fprintf(stderr,"%s\n",result.message);
    CHECK(a && sb_music_asset_loop_eligible(a)==expected);
    /* The actual synth, including filter, must reach the same boundary state.
     * Repeat with one-sample and whole-boundary blocks. Ineligible still plays. */
    for(unsigned split=0;split<2;++split) {
        sb_music_sequence_t seq={0};
        CHECK(sb_music_sequence_start(&seq,sb_music_asset_song(a)));
        if(split) {
            for(unsigned j=0;j<end;++j) CHECK(sb_music_sequence_render(&seq,samples,1));
        } else CHECK(sb_music_sequence_render(&seq,samples,end));
        CHECK((seq.state==SB_MUSIC_IDLE)==expected);
        CHECK(sb_music_sequence_render(&seq,samples,1024));
        CHECK(seq.state==SB_MUSIC_IDLE);
        CHECK(sb_music_sequence_render(&seq,samples,32));
        for(unsigned j=0;j<64;++j) CHECK(samples[j]==0);
    }
    sb_music_asset_free(a); return 0;
}
int main(void) {
    CHECK(!sb_music_asset_loop_eligible(NULL));
    /* Release .001 = 48 frames, final off at 4 => boundary 52. */
    for(unsigned muted=0;muted<2;++muted) for(unsigned filter=0;filter<2;++filter)
    for(unsigned retrigger=0;retrigger<2;++retrigger) {
        CHECK(!check(.001,51,false,retrigger,muted,filter,false,false));
        CHECK(!check(.001,52,true,retrigger,muted,filter,false,false));
        CHECK(!check(.001,53,true,retrigger,muted,filter,false,false));
    }
    CHECK(!check(0,4,true,false,false,true,false,false));
    CHECK(!check(1e-12,4,false,false,false,false,false,false));
    CHECK(!check(1e-12,5,true,false,false,false,false,false));
    CHECK(!check(.5/48000,5,true,false,false,false,false,false));
    CHECK(!check(1.5/48000,5,false,false,false,false,false,false));
    CHECK(!check(1.5/48000,6,true,false,false,false,false,false));
    CHECK(!check(.001,52,true,false,false,true,false,true));
    CHECK(!check(.001,51,false,false,false,true,false,true));
    CHECK(!check(10,1,true,false,false,false,true,false));
    /* Silent maximum-duration song is analyzed immediately, never rendered. */
    const char *start="{\"format\":\"sugarbee.music\",\"version\":1,\"timeline_rate\":48000,\"end_frame\":28800000,\"tracks\":[";
    const char *instrument="{\"instrument\":{\"waveform\":\"triangle\",\"attack\":0,\"decay\":0,\"sustain\":1,\"release\":0.001,\"volume\":1,\"pan\":0,\"filter\":{\"cutoff\":2000,\"bypass\":true}}}";
    snprintf(json,sizeof(json),"%s%s,%s],\"events\":[]}",start,instrument,instrument);
    sb_music_asset_t *a=sb_music_load(json,strlen(json),NULL);
    CHECK(a && sb_music_asset_loop_eligible(a)); sb_music_asset_free(a);
    /* Last track alone crosses boundary; other track and unused tracks cannot
     * hide it. Equality and one-frame lateness at maximum end avoid overflow. */
    for(unsigned late=0;late<2;++late) {
        snprintf(json,sizeof(json),"%s%s,%s],\"events\":["
            "{\"frame\":0,\"track\":1,\"type\":\"note_on\",\"hz\":440},"
            "{\"frame\":0,\"track\":2,\"type\":\"note_on\",\"hz\":220},"
            "{\"frame\":4,\"track\":1,\"type\":\"note_off\"},"
            "{\"frame\":%u,\"track\":2,\"type\":\"note_off\"}]}",
            start,instrument,instrument,28800000-48+late);
        a=sb_music_load(json,strlen(json),NULL);
        CHECK(a && sb_music_asset_loop_eligible(a)==!late); sb_music_asset_free(a);
    }
    puts("Loop eligibility checks passed"); return 0;
}
