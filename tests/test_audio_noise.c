#include "../src/sb_audio_noise.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "Noise line %d: %s\n", __LINE__, #e); return 1; } } while (0)
static float whole[4096], chunks[4096];
int main(void) {
    sb_audio_noise_t noise = {0}, other = {0};
    /* Fixed vectors freeze the algorithm, advancement order and float mapping. */
    const uint32_t states[] = {270369, 67634689, 2647435461u, 307599695,
                              2398689233u, 745495504, 632435482, 435756210};
    const int32_t numerators[] = {-16775103, -16248819, 3905873, -14374093,
                                 1962543, -10953033, -11836313, -13372871};
    for (unsigned i = 0; i < 8; ++i) {
        float sample;
        CHECK(sb_audio_noise_render(&noise, &sample, 1));
        CHECK(noise.state == states[i]);
        CHECK(sample == (float)numerators[i] / 16777216.0f);
    }
    const uint32_t seeds[] = {0, 1, 123456789, UINT32_MAX, UINT32_C(0x80000000)};
    for (unsigned seed = 0; seed < sizeof(seeds)/sizeof(seeds[0]); ++seed) {
        CHECK(sb_audio_noise_reset(&noise, seeds[seed]));
        CHECK(sb_audio_noise_reset(&other, seeds[seed]));
        CHECK(sb_audio_noise_render(&noise, whole, 4096));
        for (size_t i = 0; i < 4096; i += 37) {
            size_t n = 4096-i < 37 ? 4096-i : 37;
            CHECK(sb_audio_noise_render(&other, chunks+i, n));
        }
        CHECK(memcmp(whole, chunks, sizeof(whole)) == 0);
        CHECK(noise.state == other.state);
        CHECK(sb_audio_noise_reset(&noise, seeds[seed]));
        CHECK(sb_audio_noise_render(&noise, chunks, 4096));
        CHECK(memcmp(whole, chunks, sizeof(whole)) == 0);
    }
    noise = (sb_audio_noise_t){0};
    CHECK(sb_audio_noise_reset(&other, 0));
    CHECK(other.state == SB_AUDIO_NOISE_DEFAULT_SEED);
    CHECK(sb_audio_noise_render(&noise, whole, 4096));
    CHECK(sb_audio_noise_render(&other, chunks, 4096));
    CHECK(memcmp(whole, chunks, sizeof(whole)) == 0);
    CHECK(sb_audio_noise_reset(&noise, 42));
    CHECK(sb_audio_noise_render(&noise, whole, 4096));
    CHECK(memcmp(whole, chunks, sizeof(whole)) != 0);
    /* Calls to another generator cannot disturb this generator's sequence. */
    sb_audio_noise_t saved = noise;
    CHECK(sb_audio_noise_render(&other, chunks, 4096));
    CHECK(noise.state == saved.state);
    CHECK(sb_audio_noise_render(&noise, whole, 4096));
    CHECK(sb_audio_noise_render(&saved, chunks, 4096));
    CHECK(memcmp(whole, chunks, sizeof(whole)) == 0);
    uint32_t before = noise.state;
    float sentinel = 12.5f;
    CHECK(!sb_audio_noise_reset(NULL, 1));
    CHECK(!sb_audio_noise_render(NULL, &sentinel, 1));
    CHECK(!sb_audio_noise_render(&noise, NULL, 1));
    CHECK(!sb_audio_noise_render(&noise, &sentinel, SIZE_MAX));
    CHECK(noise.state == before && sentinel == 12.5f);
    CHECK(sb_audio_noise_render(&noise, NULL, 0));
    CHECK(noise.state == before);
    noise = (sb_audio_noise_t){0};
    CHECK(sb_audio_noise_render(&noise, NULL, 0));
    CHECK(noise.state == 0);
    /* Deterministic statistical smoke checks, not a proof of white spectrum.
     * Uniform [-1,1] has mean zero and mean square 1/3. Check short-lag
     * correlation too, to catch constant/alternating/strongly biased output. */
    CHECK(sb_audio_noise_reset(&noise, 123456789));
    double sum = 0, squares = 0, lag1 = 0, lag17 = 0;
    float history[17] = {0};
    unsigned positive = 0;
    const unsigned count = 1048576;
    for (unsigned base = 0; base < count; base += 4096) {
        CHECK(sb_audio_noise_render(&noise, whole, 4096));
        for (unsigned j = 0; j < 4096; ++j) {
            unsigned i = base+j;
            double value = whole[j];
            CHECK(isfinite(value) && value > -1 && value < 1);
            sum += value; squares += value*value;
            if (value > 0) ++positive;
            if (i) lag1 += value*history[(i-1)%17];
            if (i >= 17) lag17 += value*history[i%17];
            history[i%17] = whole[j];
        }
    }
    CHECK(fabs(sum/count) < .005);
    CHECK(fabs(squares/count - 1.0/3.0) < .005);
    CHECK(fabs(lag1/(count-1)) < .005);
    CHECK(fabs(lag17/(count-17)) < .005);
    CHECK(positive > count*.49 && positive < count*.51);
    puts("Standalone white-noise checks passed");
    return 0;
}
