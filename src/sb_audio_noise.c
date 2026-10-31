#include "sb_audio_noise.h"

bool sb_audio_noise_reset(sb_audio_noise_t *noise, uint32_t seed) {
    if (!noise) return false;
    noise->state = seed ? seed : SB_AUDIO_NOISE_DEFAULT_SEED;
    return true;
}

bool sb_audio_noise_render(sb_audio_noise_t *noise, float *output, size_t samples) {
    if (!noise || samples > SIZE_MAX / sizeof(float) || (!output && samples))
        return false;
    for (size_t i = 0; i < samples; ++i) {
        /* xorshift32: explicit unsigned 32-bit operations, never libc rand().
         * A zero state would stay zero, so select the defined default instead. */
        uint32_t x = noise->state ? noise->state : SB_AUDIO_NOISE_DEFAULT_SEED;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        noise->state = x;
        /* Map upper 24 bits to symmetric bin midpoints. The signed numerator
         * fits int32_t and is exactly representable in binary32; no clipping. */
        int32_t numerator = (int32_t)(x >> 8) * 2 - INT32_C(16777215);
        output[i] = (float)numerator * (1.0f / 16777216.0f);
    }
    return true;
}
