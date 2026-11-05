# Native WAV foundation

This checkpoint adds native decoding, bounded storage, one-shot playback and
mixing. It does not add Lua loading, cartridge asset lookup, loops, streaming,
WAV music, or a cartridge WAV Lab. Those remain separate work.

## Accepted input and conversion

The C decoder borrows a memory buffer, capped at **16 MiB**, matching the existing
cartridge entry cap. The caller owns and frees that source buffer. Accepted
files are little-endian RIFF/WAVE, format tag 1, signed 16-bit PCM, one or two
channels, at 22,050, 44,100 or 48,000 Hz. `fmt ` may be 16 bytes or 18 bytes with
a zero extension size. Extensible/float/compressed WAV and other rates/depths
are rejected. There must be one format chunk before exactly one nonempty data
chunk. Block alignment, byte rate, complete PCM frames, RIFF/chunk sizes, and
odd-chunk padding presence are checked. Duplicate fmt/data, truncated chunks,
trailing bytes outside RIFF, and more than 1024 chunks are rejected. Unknown
chunks are skipped with bounds checks; their metadata is not interpreted.

RIFF/PCM reference: IBM/Microsoft, *Multimedia Programming Interface and Data
Specifications 1.0*, August 1991, chapters 2 and 3:
[original specification](https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/riffmci.pdf).

Decoded storage is interleaved 48 kHz float, retaining mono/stereo channels.
PCM maps −32768 to −1 and 32767 to 32767/32768. Conversion uses linear
interpolation with an integer phase calculation and holds the last source frame
at the boundary. Output length is `ceil(source_frames * 48000 / source_rate)`.
This is a simple interpolator, not a high-quality band-limited resampler; spectral
images near the original Nyquist limit are not eliminated. No normalization or
extra conversion buffer is used. 48 kHz input preserves each PCM sample value.

## Bounds and ownership

| Resource | Limit |
| --- | --- |
| Borrowed source bytes | 16 MiB |
| Decoded asset | 4 MiB |
| Retained decoded samples | 32 MiB |
| Loaded assets | 64 |
| Concurrent playbacks | 8 |

Decode allocates at most one 4 MiB output before publishing it. Thus a load may
coexist with 32 MiB retained data and 16 MiB caller-owned source: at most 52 MiB
sample/source storage plus fixed metadata and allocator overhead. No compressed
archive staging is added here. Future cartridge loading must account separately
for the archive reader's working memory.

Overlapping playbacks reference shared asset data. Unload refuses an asset used
by a playing or stopping voice. At file end or stop completion, only the playback
slot is released; decoded assets remain until unload or session teardown.
Failure stops playbacks after its fade but retains assets until control-thread
cleanup. The callback never allocates or frees. Begin/end release all assets;
destroy closes the device before freeing storage. There is no drain wait.

## Native API and results

`include/sb_wav.h` supplies standalone `sb_wav_decode`/`sb_wav_free`, usable
without a device. `include/sb_audio.h` supplies memory-buffer load, unload, play,
playing query, stop, and combined volume/pan controls. All device API calls are
host-control-thread operations; decoding happens outside the device lock.

Load/play produce nonzero 64-bit context-local handles, zero on failure. Asset
and playback IDs use a common monotonic sequence that survives session reset;
IDs are never reused during a device lifetime. Exhaustion is refused, not
wrapped. IDs cannot be transferred between devices. Native playing/stop/control
calls return false for unknown, stale, or ended playback IDs. Playback slots
cannot be hijacked by an old ID. Future Lua handle validation is separate work.

Load/play/unload return `sb_wav_result_t`; `sb_wav_result_string` provides stable
names: `ok`, `invalid_input`, `unsupported_wav`, `wav_too_large`, `out_of_memory`,
`audio_budget`, `assets_full`, `voices_full`, `invalid_handle`, `asset_in_use`,
`audio_unavailable`, `audio_rejected`, and `handles_exhausted`.
A NULL audio device returns unavailable (a missing output-handle pointer is
invalid input). Failed/closed sessions reject loading and playback. An invalid
or refused operation never steals a live slot. Full decode/validation precedes
asset-budget refusal; temporary decoded data is freed when publication fails.

Play takes finite volume 0–1 and pan −1–1. Mono uses equal-power endpoint pan
gains; stereo uses linear balance: center preserves both channels, full left
mutes right and full right mutes left. Initial gains apply immediately. Live
volume/pan edits interpolate channel gains over 480 frames (10 ms); repeated
targets do not restart the ramp. Intermediate mono gains during the ramp are
linear blends, not points constrained to a constant-power pan arc.

Stop fades from current gains to zero over 480 frames, or until the file ends,
whichever comes first. A stopping playback is still playing and retains its
slot/asset. Repeated stop and controls while stopping return false. Natural ends
are not automatically faded, so authors should prepare clean endpoints. No
start fade, looping, or per-playback pause is added.

## Mixing and lifecycle

The existing synth gain is unchanged: approximately **0.790569415** after its
fixed 1/9 bank headroom. WAV playback uses **0.025 per voice**, giving at most
0.2 across all eight voices. Bounded decoded samples, interpolation, volume,
pan/balance, and stop/session fades each preserve that bound. Combined per-channel
peak is at most approximately **0.99057**, allowing float rounding margin below
1. No active-count normalization, ducking, limiter, or final clamp is needed.
Legacy native diagnostics remain quieter; the WAV allowance is the same.

The common session gain applies after synth+WAV mixing. Pause fades for 480
frames then freezes playback positions and gain ramps along with the synth bank;
resume continues with a fade-in. Failure rejects new controls and fades out.
Fully paused failure stops immediately. Paused shutdown stays silent even if it
starts WAVs. Teardown can cut short fades and cannot retract already queued
device samples. A stop while paused completes only after resume or teardown.

## Verification and listening

`wav_core` checks all rate/channel combinations, exact sample conversion,
malformed/truncated/duplicate chunks, odd padding, allocation failure, header
mutations, decoded size and pool budgets, slot exhaustion, stale handles, and
stop lifetime. `wav_sdl` drives the real paused dummy-device callback to check
pan/balance, ramp deadlines and exact stop samples, callback partitioning,
complete pause freeze, resume, failure, fresh-session invalidation, and combined
nine-synth/eight-WAV headroom. `wav_output` runs the generated listening sequence
with dummy audio (execution coverage, not audible validation).

```sh
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/sbee-wav-test
```

The roughly eight-second tool generates PCM WAV buffers in memory, then uses the
public loader. Expect:

1. Centered 440 Hz mono at 22.05 kHz source rate, ending naturally.
2. Stereo 44.1 kHz: low tone left, high tone right, then left/right balance.
3. Mono 48 kHz: left to right, quieter center, then stop.
4. Two WAVs plus a 220 Hz synth: pause silence, resume, then failure silence.
5. A fresh session's tone, then silent paused shutdown.

Listen for correct pitch/channel placement, audible but conservative levels,
clean control transitions, and no stuck sounds or bursts during pause/cleanup.
Then check Pong sounds, musical/basic Synth Labs, Pac-Man, pause/resume,
exit/relaunch, and Exit to system status 0. Desktop acceptance (2026-11-04):
the user confirmed all checks passed on coastline for the implementation based
on `79b631d`. No Lua API changes are included.

Pi validation (2026-11-04), user-reported at `b292f82`: all 86 tests passed;
WAV listening, Pong, Synth Labs, Pac-Man, pause/resume, exit and launch all
passed. Exit to system returned 0. Desktop and Pi validation are complete.

Development verification (2026-11-04): Debug build completed without compiler
warnings; all 86 tests passed. After preserving the idle-silence fast path, all
eight affected audio tests also passed. AddressSanitizer and UndefinedBehaviorSanitizer
passed standalone decoder/pool and SDL lifecycle runs. LeakSanitizer could not
run under the environment's tracing setup, so those runs disabled leak detection;
they are not a leak-check result.
