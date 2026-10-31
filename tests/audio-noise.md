# Standalone white-noise source

This checkpoint provides a pure C mono pseudo-random noise generator. It has no
SDL, Lua, allocation, global random state, bank integration, or listening tool.
It is an audio source, not a cryptographic random generator.

## Sequence and reset contract

`sb_audio_noise_t` owns one 32-bit unsigned state. Reset with a nonzero seed
restarts that sequence immediately. Seed zero maps to default seed 1; a
zero-initialized object produces the same sequence on its first rendered sample.
Reset does not fade or stop sound. There is no idle, envelope, volume, or pan
state in this component. These belong to later voice integration.

Each sample advances xorshift32 with left 13, right 17, left 5 shifts, in that
order. The upper 24 output bits `b` map to `(2*b - 16777215) / 16777216`.
These symmetric bin midpoints lie strictly between -1 and +1 and are exactly
representable on the binary32 target platforms. There is no gain or clamp.
The fixed sequence is independent of libc random functions and byte order.
Different seeds select different starting states; they do not promise
mathematically independent streams. Future bank integration must choose seeds
explicitly rather than accidentally starting every voice with seed 1.

Render replaces a mono buffer and advances once per sample, with no pitch or
sample-rate parameter. Splitting a render into several blocks preserves the
exact output and ending state. Zero samples are a no-op, including for an
uninitialized zero state, and permit null output. Null state, null output for
nonzero samples, and overflowing buffer sizes fail without modifying output
or state. The caller owns buffer capacity and synchronization; the output
must not overlap generator storage.

## Checks

```sh
ctest --test-dir build -R '^audio_noise$' --output-on-failure
```

Tests cover fixed state/sample vectors, multiple seeds, reset replay, zero seed,
zero initialization, generator isolation, block partitioning, invalid inputs,
and a million-sample statistical smoke check. Samples must be finite and bounded;
mean, mean square, sign balance, and lag-1/lag-17 correlation have broad fixed
tolerances. These deterministic checks catch obvious bias and repetition; they
do not establish a perfectly flat spectrum or replace future listening tests.

The component builds independently of the SDL/Lua host:

```sh
cmake -S . -B /tmp/sugarbee-noise-standalone -DSUGARBEE_BUILD_HOST=OFF -DSUGARBEE_BUILD_PACKER=OFF
cmake --build /tmp/sugarbee-noise-standalone --target test_audio_noise
ctest --test-dir /tmp/sugarbee-noise-standalone -R '^audio_noise$' --output-on-failure
```

Bank integration, per-voice seed/retrigger policy, a native listening sequence,
and Pi validation remain later checkpoints. Existing triangle and diagnostic
playback behavior is unchanged.
