# Isolated pitch candidate: Pi render-cost checkpoint

Historical investigation checkpoint: `de04c52`. Runtime integration is now
tracked in [runtime validation](audio-piano-runtime.md). Reproduce the isolated
source-patching scripts from `de04c52`; they intentionally refuse the integrated
source tree. Statements below about unchanged runtime describe that checkpoint.

Prepared 2026-11-13. Runtime unchanged; Pi offline render-cost screening passed.
Live SDL/gameplay validation remains pending.
The uncommitted source bundle supports validation without touching the Pi checkout.
Python 3 standard library, a C99 compiler and GNU-compatible linker are required;
no NumPy, SDL development package, device access, or root privileges are needed.

## Workload and interpretation

`tests/experiments/pitch_benchmark.py` compiles baseline and candidate separately
with `-std=c99 -O2 -Wall -Wextra -Werror`, using temporary source copies. Shared
`pitch_candidate.py` applies checked substitutions, refusing unexpected source
text. Only temporary oscillator, bank and sequencer frequency guards plus the
pulse correction are changed. Lua/loader contracts are deliberately untouched.

Three configurations run serially: baseline 110–880 Hz, candidate 110–880 Hz,
and candidate A0–C8. Each tests nine sustained pulses, mixed triangle/saw/pulse,
and repeated pulse/saw transitions with pitch/width/filter changes. Every case
also runs eight looping stereo WAV playbacks from separate 128 KiB assets.
Waveform transitions start only after previous transitions finish. Controls are
included in transition timing. This is a synthetic sustained rendering workload,
not a maximum-event-density song or an exhaustive worst-case workload.

The native music sequence/transport, bank and WAV renderer execute once per
frame, following the normal session mixing order and gains. An output buffer is
cleared each block. We use monotonic elapsed time around each block, including
control updates, with 32 warmup blocks. Output validation and statistics are
outside timing. GNU linker wrappers count malloc/calloc/realloc/free calls
through the linked project code during measured work. All nine synth voices and
eight WAV playbacks must still be active after each case; output must be finite,
non-silent and within ±1. These checks are not compiled away with NDEBUG.

Results include mean, p99, maximum, percentages of `frames / 48000`, observed
misses and heap calls. Percentiles use nearest rank. The script saves results
and source hashes/compiler/platform metadata to `build/pitch-benchmark/`.

This is an **offline render-cost proxy**, not the actual SDL callback: it excludes
SDL/backend scheduling, concurrent game load, device underrun detection, pause
and lifecycle edge cases, and event bursts. Output is discarded, not played.
Normal callback rendering is allocation-free by construction; the wrapper check
only verifies the exercised project path. CPU thermal/governor conditions and
OS preemption affect measurements. Do not infer hardware acceptance from desktop
measurements or treat a short observed maximum as a worst-case guarantee.

Provisional screening target: zero observed deadline misses and heap calls,
p99 below 25% and maximum below 50% of the chosen buffer deadline. These are
conservative investigation targets, not an established real-time guarantee.
Inspect candidate/baseline timings on the same machine; rerun unexplained outliers
before accepting or rejecting. Real SDL/Pi playback under game load remains a
later integration gate.

## Run on the development host

```sh
python3 tests/experiments/pitch_benchmark.py --frames 512 --blocks 1000
python3 tests/experiments/pitch_benchmark.py --package build/pitch-benchmark/pi-pitch-benchmark.tar.gz
```

512 is an explicit test size, **not a freshly verified Pi device setting**.
Use the actual obtained frame count from the Pi engine's SDL startup log; adjust
`--frames` if it differs. 512 frames at 48 kHz has a 10.667 ms deadline.

Desktop verification at 512 frames / 1000 measured blocks per case: all nine
cases passed, zero misses and heap calls. Candidate-wide p99 was 0.1357 ms
(pulse), 0.1355 ms (mixed), 0.2260 ms (transitions). Its largest observed block
was 0.2954 ms (2.77% of deadline). This is desktop evidence only. The prior
oscillator and edge/DC checks passed after sharing the temporary patch helper.
The packaged sources were also extracted into a clean temporary directory and
compiled/run independently at 256 frames / 100 blocks per case.

## Transfer uncommitted snapshot and run on Pi

On the development host, replace `PI_HOST` with the Pi's hostname/address:

```sh
scp build/pitch-benchmark/pi-pitch-benchmark.tar.gz bofh@PI_HOST:~/
```

On the Pi (serial benchmark; no CTest parallelism):

```sh
pitch_bench_dir=$(mktemp -d "$HOME/sugarbee-pitch-bench.XXXXXX")
tar -xzf "$HOME/pi-pitch-benchmark.tar.gz" -C "$pitch_bench_dir"
python3 "$pitch_bench_dir/tests/experiments/pitch_benchmark.py" --frames 512 --blocks 1000
cat "$pitch_bench_dir/build/pitch-benchmark/results.txt"
```

Keep the extraction directory until results are reviewed. Report its results,
actual device frame count, and whether the Pi was otherwise idle or under load.
No checkout modification, engine installation, or commit is involved. This does
not lift the hold on production integration.

## Pi results — 2026-11-13

User returned results and metadata via removable media. Metadata identifies
Raspberry Pi 4 Model B Rev 1.5, aarch64, GCC 14.2.0. All 43 input hashes match
the local source snapshot. Run: 512 frames, 1000 measured blocks per workload,
10.6667 ms nominal deadline. Actual SDL obtained buffer size and background
load/thermal conditions were not reported; this assessment is for the explicit
512-frame offline workload.

| Candidate A0–C8 workload | Mean ms | p99 ms | Max ms | Max deadline use |
| --- | ---: | ---: | ---: | ---: |
| Nine pulses + eight WAVs | 0.5493 | 0.5691 | 0.6414 | 6.01% |
| Mixed waveforms + eight WAVs | 0.3887 | 0.4040 | 0.4784 | 4.49% |
| Transitions + eight WAVs | 0.7518 | 0.7791 | 0.8100 | 7.59% |

All nine baseline/candidate workloads report zero misses and zero timed heap
calls. Candidate-wide worst p99 uses 7.30% of the nominal deadline, comfortably
below the provisional 25% gate; worst observed block uses 7.59%, below 50%.
Same-band candidate mean cost increases about 7.3% for pulse, 4.2% for mixed,
and 9.4% for transitions relative to baseline in this run. The absolute margin
remains ample for this workload; a single sequential run is not a controlled
statistical estimate of overhead or a guarantee under game load.

Raw timings and a metadata summary with matching source hashes are preserved in
[results](experiments/results/pi4-pitch-2026-11-13.txt) and
[metadata](experiments/results/pi4-pitch-2026-11-13.json). Device serial is omitted.
The render-cost feasibility gate passes. No production implementation, live
SDL acceptance, commit or push is implied. The endpoint contract was subsequently
agreed: 27.5–4186.01 Hz inclusive (see the feasibility report). Next is bounded
runtime integration with full regression and desktop/Pi playback validation.
