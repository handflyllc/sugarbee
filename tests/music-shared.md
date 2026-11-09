# Native shared-voice checkpoint

Internal owner: `src/sb_music_shared.h`. This is an offline native checkpoint;
production SDL audio, Lua, cartridge loading and the private host-pause adapter
are not yet connected to it. Existing private sequencer/host operations remain.

## Contract

- Initialize fresh storage with `sb_music_shared_init`. It owns one nine-voice
  bank and one player. All calls are serialized by the caller. Do not copy live
  ownership into another independently running player.
- Start requires exactly one distinct physical voice in 1..9 per logical track,
  including unused tracks. The mapping is copied; song/instrument/event arrays
  remain borrowed and immutable until idle/reset.
- Validate the entire song, loop eligibility, mapping and voice availability
  before publishing changes. Sounding, releasing and stopping requested voices
  are unavailable. Busy or invalid starts leave all state unchanged.
- On successful start, requested idle voices receive fresh silent defaults plus
  the song's instrument settings. Old seed/pan/filter/oscillator settings do not
  leak into playback. As in the existing synth, an unspecified noise seed uses
  the physical slot's fixed default; comparisons must use the same mapping.
- Reservations last through rests, mute and finishing. All direct voice mutation
  calls in this owner's interface refuse reserved slots without changing state.
  Queries remain allowed. Unreserved direct voices retain normal bank behavior.
- The raw bank and embedded structs are internal implementation state, not a
  security boundary. Bypassing the owner with raw bank calls or field writes is
  unsupported. Production Lua reservation enforcement is not claimed yet.
- One render advances every physical voice exactly once. Music gain applies only
  to the reserved contribution, with the existing 480-frame ramp and mute-time
  behavior. Both music and effects retain fixed 1/9 headroom, with no boost,
  clipping, extra voices or active-count normalization. Shared grouped summation
  can round differently from summing separately rendered float buffers.
- Loop restoration copies only reserved slots, including all oscillator, noise,
  filter, envelope, waveform-transition and retained settings. Other slots keep
  progressing. No shared whole-bank snapshot is restored.
- Natural completion and stop retain ownership until music voices are silent;
  unrelated active effects do not delay music completion. Cleanup resets only
  music-owned slots to silent defaults, releases reservations and clears the
  borrowed song. A new play starts at gain 1. No prior settings are restored.
- `sb_music_shared_reset` is immediate music-only cleanup and preserves effects.
  `init` is whole-owner initialization/teardown and must not be used as music stop
  in a running shared bank. Stop before onset/within a silent gap can clean up
  immediately. Repeated stops do not extend fades.
- Invalid render buffers fail unchanged; zero-frame rendering does not dispatch.
  Continue calling the shared renderer while music is idle to advance effects.

Private `sb_music_host` is not a shared mixer: do not insert mapped playback into
it or render the shared bank a second time. The subsequent
[combined native host checkpoint](music-shared-pause.md) adds pause directly to
this owner using common lifecycle code. Production routing remains deferred.

## Validation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
./build/test_music_shared
ctest --test-dir build --output-on-failure
```

The native test exercises nonidentity mapping, complete guarded control refusal,
invalid/duplicate/short mappings, late invalid instruments, busy release/stop
voices, copied mapping lifetime, full nine-voice reservation, empty tracks,
one-shot tails, loop eligibility, stop/reset cleanup and replay. It compares
music plus an independently rendered effect to the shared output, and compares
whole/single-frame rendering exactly. Unrelated active noise-to-pulse transition
and filter state, plus silent retained settings, survive music loops and gain
changes. Music mute preserves the independently rendered effect.

At the original ownership checkpoint, desktop owner/sequencer size was 7392
bytes; bank: 3640 bytes. See the combined checkpoint for current owner size. Native sequence layout
changed, so pinned consumers must rebuild. No stable ABI is promised. Start uses
bounded stack scratch; rendering and controls introduce no allocations or I/O.

Desktop Debug build: no warnings. Full suite **102/102 passed** (106.68 s).
All 21 focused music/bank/session/sample checks passed. Nine native music/bank
ASan/UBSan checks passed with leak detection disabled under tracing. Diff and
whitespace review passed. Pi acceptance reported by the user on 2026-11-09 at
synchronized `3f1f849698ada3a24ed0c9a3a7c8748a5161d3e4`: `test_music_shared`
returned exit 0; full suite passed 102/102 with no failures.
This checkpoint has no new SDL listening mode; existing
private music/session diagnostics remain regression checks. No new audible
coexistence acceptance or production integration is claimed.

Engine validation is complete. Next priority: SugarBeats bounded offline mapped-voice,
control-refusal, parity and asset-lifetime checks. Do not add preview controls,
Lua bindings or production mixer integration in the follower checkpoint.
