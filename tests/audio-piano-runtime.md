# A0–C8 runtime integration

2026-11-13, based on investigation commit `de04c52`. Runtime edits are
uncommitted; desktop validation is accepted, and Pi validation remains pending.
The earlier Pi benchmark measured the isolated candidate, not this exact build.

## Contract and implementation

All pitched sources accept finite **27.5–4186.01 Hz inclusive**. Exact
C8 (~4186.009045 Hz) and C8 rounded to two decimals both fit. Accepted values
are used unchanged; no rounding, clamping, snapping or boundary tolerance.
`src/sb_audio_pitch.h` supplies shared constants and the native predicate.
Standalone triangle/saw/pulse, bank starts/edits/retained pitch, Lua note-on,
prepared music and JSON loading use the same limits. Invalid requests preserve
existing refusal/atomicity semantics. Noise still has no audible pitch control.

Pulse now sums corrections from past and future signed edge motion, allowing
backward or stationary falling edges and overlapping narrow-pulse corrections.
Stored phase-step limits derive from the pitch range; falling-history limits
also include maximum 0.9/480 width motion plus 16 double epsilons for ramp
arithmetic. This internal arithmetic allowance does not widen frequency input
acceptance. The default width, 5–95% width range and 480-frame ramp are unchanged.

Triangle and saw synthesis algorithms, nine voices, fixed mixing gains, 48 kHz,
ADSR and cartridge/music format are unchanged. Older engines reject newly
out-of-range music; consumer pins need the accepted updated engine. No
SugarBeats code or dependency pin is changed by this checkpoint.

## Verification

Build and run serially:

```sh
cmake --build build -j4
env -u CTEST_PARALLEL_LEVEL ctest --test-dir build --output-on-failure
```

Native tests now cover both endpoints, exact C8, fractions, adjacent rejected
doubles, nonfinite inputs, atomic rejection during fades, noise retained pitch,
nine-voice headroom, and sequencer/Lua/JSON traversal. Old-range spectral tests
remain at their original thresholds. Pulse coverage includes independent
triangular-kernel quadrature, exact zero-motion crossing, signed history,
100 pitch/width retargets with exact partition invariance, and corrupt-history
rejection without state/output mutation.

Supplemental desktop checks loaded the actual production pulse/saw C alongside
`de04c52` sources in temporary shared libraries: sampled old-band raw and
conditioned width sweeps remained bit-identical. Production also passed the
previous 4095 numerical edge comparisons, three exact stationary cases,
500 seeded retargets and settled low-note native-filter checks. Maximum
quadrature discrepancy was 0.00006583; maximum absolute filtered mean about
1.3e-9. These are sampled bounds, not a universal proof or listening substitute.

The first development suite found five obsolete rejection assertions for
109/881 Hz. Those now test the new out-of-range boundaries. Final serial-suite
result: all 130 tests passed on the development host. No automatic test proves audible
quality; SDL dummy output only verifies API execution and cleanup.

## Manual acceptance

On the development host, then on Pi against the same accepted source revision:

```sh
./build/sbee-pitch-test --piano
```

About 25 seconds, with printed section labels:

1. Triangle, saw and 50% pulse each play 27.5, 110, 880, 1760, 3520 and
   4186.01 Hz, with fades and gaps. Check expected rising pitch, recognizable
   timbre, clean releases and no unexpected silence. Small speakers may not
   reproduce the A0 fundamental; do not increase global gain to compensate.
2. Pulse changes between A0 and the C8 ceiling while width sweeps between
   5% and 95%. Intentional timbre/loudness changes and abrupt pitch jumps are
   expected; check for persistent buzz, unintended dropouts or stuck notes.
3. C8 triangle → saw → pulse → noise → A0 triangle. Confirm the low return
   and normal exit. The no-argument tool preserves the previous listening sequence.

Confirm exit status 0. For Pi acceptance also run the full serial suite, record
SDL's actual obtained buffer/driver, check existing music pause/resume/loop and
cartridge exit/relaunch, and play the existing Pong/Pac-Man cartridges before
Exit to system. Existing CTest cartridge fixtures remain unchanged to exercise
old-content compatibility. Pi benchmark headroom is supporting evidence; live
playback under game load remains required.

## Status

- Investigation: desktop listening and Pi offline benchmark accepted before integration.
- Runtime desktop build: passed without compiler warnings.
- Runtime serial suite: **130/130 passed** on desktop, exit 0, run serially.
- Runtime desktop listening: accepted by the user on 2026-11-13 ("Desktop validation is good").
- Runtime Pi build/tests/listening: pending.
- Commit/push: not performed; Git metadata is read-only in this session.
- Paused at user request after desktop acceptance. Resume with runtime commit/transfer,
  then Pi build, full serial tests, piano listening and cartridge lifecycle checks.
