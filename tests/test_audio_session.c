/* Paused dummy device: drive the real callback deterministically. */
#include "../src/sb_audio_sdl.c"
#include <math.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr,"Session line %d: %s\n",__LINE__,#e); return 1; } } while (0)
static float out[4096], expected[4096];
static void render(sb_audio_t *a, unsigned frames) {
    audio_output(a,(Uint8 *)out,(int)(frames*2*sizeof(float)));
}
static bool silence(unsigned frames) {
    for (unsigned i=0;i<frames*2;++i) if (out[i]!=0) return false;
    return true;
}
static bool populate(sb_audio_t *a) {
    for (unsigned v=1;v<=9;++v) {
        if (!sb_audio_voice_configure(a,v,0,0,1,.2) ||
            !sb_audio_voice_volume(a,v,1) || !sb_audio_voice_pan(a,v,-1) ||
            !sb_audio_voice_filter_bypass(a,v,false)) return false;
        if (!(v%3==0 ? sb_audio_voice_noise_on(a,v) : v%3==1 ?
            sb_audio_voice_saw_on(a,v,440) : sb_audio_voice_pulse_on(a,v,440))) return false;
    }
    return true;
}
int main(void) {
    sb_audio_session_begin(NULL); sb_audio_session_pause(NULL,true);
    sb_audio_session_fail(NULL); sb_audio_session_end(NULL);
    sb_audio_session_prepare_shutdown(NULL);
    sb_audio_t *a=sb_audio_create(); CHECK(a);
    SDL_PauseAudioDevice(a->device,1);
    sb_audio_session_end(a);
    CHECK(!sb_audio_voice_note_on(a,1,440));
    CHECK(!sb_audio_voice_filter_cutoff(a,1,500));
    sb_audio_session_begin(a);
    CHECK(a->mode==AUDIO_SESSION && a->fade.value==1 && !a->paused);
    CHECK(!sb_audio_start_test_tone(a) && !sb_audio_start_triangle(a,440));
    CHECK(populate(a));
    render(a,113);
    CHECK(sb_audio_voice_waveform(a,1,SB_AUDIO_VOICE_TRIANGLE));
    CHECK(sb_audio_voice_filter_cutoff(a,2,20));
    CHECK(sb_audio_voice_pulse_width(a,2,.95));
    CHECK(sb_audio_voice_volume(a,3,.4));
    sb_audio_bank_t reference=a->bank;
    sb_audio_session_pause(a,true);
    CHECK(a->fade.left==480);
    CHECK(sb_audio_bank_render(&reference,expected,480));
    render(a,479);
    for (unsigned i=0;i<479;++i) for (unsigned ch=0;ch<2;++ch)
        CHECK(fabs(out[2*i+ch]-expected[2*i+ch]*bank_diagnostic_gain*(1-i/480.0))<1e-7);
    CHECK(a->fade.left==1);
    sb_audio_session_pause(a,true); CHECK(a->fade.left==1);
    render(a,1);
    CHECK(a->fade.left==0 && a->fade.value==0);
    CHECK(memcmp(&a->bank,&reference,sizeof reference)==0);
    render(a,2048); CHECK(silence(2048));
    CHECK(memcmp(&a->bank,&reference,sizeof reference)==0); /* ALL bank state frozen. */
    sb_audio_session_pause(a,false);
    CHECK(sb_audio_bank_render(&reference,expected,480));
    render(a,480);
    for (unsigned i=0;i<480;++i) for (unsigned ch=0;ch<2;++ch)
        CHECK(fabs(out[2*i+ch]-expected[2*i+ch]*bank_diagnostic_gain*i/480.0)<1e-7);
    CHECK(a->fade.value==1 && !a->fade.left);
    CHECK(memcmp(&a->bank,&reference,sizeof reference)==0);
    /* Rapid reversal starts at current gain, without resetting voice state. */
    sb_audio_session_pause(a,true); render(a,120);
    CHECK(a->fade.value==.75);
    sb_audio_session_pause(a,false); CHECK(a->fade.from==.75 && a->fade.left==480);
    render(a,71);
    sb_audio_session_pause(a,false); CHECK(a->fade.left==409);
    /* Callback partitioning is transparent across the fade boundary. */
    sb_audio_t copy=*a;
    render(a,1000); memcpy(expected,out,2000*sizeof(float));
    float chunked[2000];
    for (unsigned i=0;i<1000;i+=37) {
        unsigned n=1000-i<37?1000-i:37;
        audio_output(&copy,(Uint8 *)(chunked+2*i),(int)(n*2*sizeof(float)));
    }
    CHECK(memcmp(expected,chunked,sizeof chunked)==0);
    CHECK(memcmp(&a->bank,&copy.bank,sizeof a->bank)==0);
    /* Failure is sticky, rejects EVERY bank control, and clears after 480 frames. */
    sb_audio_session_fail(a);
    reference=a->bank;
    CHECK(!sb_audio_voice_configure(a,1,0,0,1,0));
    CHECK(!sb_audio_voice_note_on(a,1,440));
    CHECK(!sb_audio_voice_saw_on(a,1,440));
    CHECK(!sb_audio_voice_pulse_on(a,1,440));
    CHECK(!sb_audio_voice_noise_on(a,1));
    CHECK(!sb_audio_voice_start_selected(a,1,440));
    CHECK(!sb_audio_voice_start_selected_noise(a,1));
    CHECK(!sb_audio_voice_waveform(a,1,SB_AUDIO_VOICE_NOISE));
    CHECK(!sb_audio_voice_pitch(a,1,880));
    CHECK(!sb_audio_voice_volume(a,1,0)); CHECK(!sb_audio_voice_pan(a,1,1));
    CHECK(!sb_audio_voice_filter_cutoff(a,1,20)); CHECK(!sb_audio_voice_filter_bypass(a,1,true));
    CHECK(!sb_audio_voice_seed(a,1,123)); CHECK(!sb_audio_voice_pulse_width(a,1,.05));
    CHECK(!sb_audio_voice_note_off(a,1)); CHECK(!sb_audio_voice_stop(a,1));
    CHECK(!sb_audio_voice_active(a,1));
    CHECK(memcmp(&a->bank,&reference,sizeof reference)==0);
    render(a,137); unsigned remaining=a->fade.left;
    sb_audio_session_fail(a); sb_audio_session_pause(a,false);
    CHECK(a->mode==AUDIO_FAILED && a->fade.left==remaining);
    render(a,remaining); CHECK(!bank_active(a));
    sb_audio_bank_reset(&reference);
    CHECK(memcmp(&a->bank,&reference,sizeof reference)==0);
    render(a,1024); CHECK(silence(1024));
    /* Failure while fully paused clears immediately, never resumes. */
    sb_audio_session_begin(a); CHECK(populate(a));
    sb_audio_session_pause(a,true); render(a,480);
    sb_audio_session_fail(a); CHECK(!a->fade.left && !bank_active(a));
    /* Shutdown while pause fade is pending: controls stay available but silent. */
    sb_audio_session_begin(a); CHECK(populate(a));
    sb_audio_session_pause(a,true); render(a,20);
    sb_audio_session_prepare_shutdown(a);
    CHECK(a->fade.value==0 && !a->fade.left);
    CHECK(sb_audio_voice_start_selected(a,1,880));
    reference=a->bank;
    render(a,1024); CHECK(silence(1024));
    CHECK(memcmp(&a->bank,&reference,sizeof reference)==0);
    sb_audio_session_end(a);
    CHECK(a->mode==AUDIO_CLOSED && !a->paused && !bank_active(a));
    CHECK(!sb_audio_voice_note_on(a,1,440));
    sb_audio_session_begin(a);
    CHECK(sb_audio_voice_note_on(a,1,440));
    CHECK(a->bank.filters[0].wet==0 && a->bank.filters[0].cutoff==2000);
    CHECK(a->bank.voices[0].volume==.5 && a->bank.seeds[0]==0);
    /* Cleanup cannot depend on callback progress: the SDL device is paused. */
    sb_audio_session_end(a); sb_audio_session_end(a);
    render(a,1024); CHECK(silence(1024));
    sb_audio_destroy(a);
    puts("Cartridge audio session checks passed");
    return 0;
}
