#include "../src/sb_audio_tone.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "Tone check failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)
static float whole[(SB_AUDIO_TONE_FRAMES + 100) * 2];
static float chunked[(SB_AUDIO_TONE_FRAMES + 100) * 2];
int main(void) {
    sb_audio_tone_t tone = {0};
    float idle[16];
    memset(idle, 0xff, sizeof(idle));
    sb_audio_tone_render(&tone, idle, 8);
    for (unsigned i = 0; i < 16; ++i) CHECK(idle[i] == 0.0f);
    sb_audio_tone_start(&tone);
    sb_audio_tone_render(&tone, whole, SB_AUDIO_TONE_FRAMES - 1);
    CHECK(tone.active && tone.frame == SB_AUDIO_TONE_FRAMES - 1);
    sb_audio_tone_render(&tone, whole + 2 * (SB_AUDIO_TONE_FRAMES - 1), 101);
    CHECK(!tone.active && tone.frame == SB_AUDIO_TONE_FRAMES);
    CHECK(whole[0] == 0 && whole[2 * (SB_AUDIO_TONE_FRAMES - 1)] == 0);
    for (unsigned i = SB_AUDIO_TONE_FRAMES; i < SB_AUDIO_TONE_FRAMES + 100; ++i)
        CHECK(whole[2*i] == 0 && whole[2*i+1] == 0);
    unsigned crossings = 0;
    float peak = 0;
    for (unsigned i = 0; i < SB_AUDIO_TONE_FRAMES; ++i) {
        float value = whole[2*i];
        CHECK(isfinite(value) && fabsf(value) <= 0.050001f);
        CHECK(value == whole[2*i+1]);
        if (fabsf(value) > peak) peak = fabsf(value);
        if (i && whole[2*(i-1)] <= 0 && value > 0) ++crossings;
        if (i < 480) CHECK(fabsf(value) <= 0.05f * i / 480 + 0.000001f);
        if (i >= 47520) CHECK(fabsf(value) <= 0.05f * (47999-i) / 480 + 0.000001f);
    }
    CHECK(peak > 0.049f && crossings == 440);
    sb_audio_tone_start(&tone);
    size_t position = 0;
    while (position < SB_AUDIO_TONE_FRAMES + 100) {
        size_t frames = position % 1031 + 1;
        if (frames > SB_AUDIO_TONE_FRAMES + 100 - position)
            frames = SB_AUDIO_TONE_FRAMES + 100 - position;
        sb_audio_tone_render(&tone, chunked + 2 * position, frames);
        position += frames;
    }
    CHECK(memcmp(whole, chunked, sizeof(whole)) == 0);
    puts("Tone sample checks passed");
    return 0;
}
