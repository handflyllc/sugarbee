# WAV Lab

Three deterministic generated WAVs exercise cartridge loading and playback:
a 22.05 kHz mono 440 Hz chime and a 44.1 kHz stereo sound with a low tone on the
left and a higher tone on the right. Each lasts two seconds and decays to silence.
The third is a steady 48 kHz mono 440 Hz tone, authored with an integer number
of cycles over two seconds and played in a whole-sound loop.
They are original generated samples, not external recordings. Source is
`tests/wav-lab/generate.c`; generated files remain under ignored `build/wav-lab/`.
The prepared cartridge includes the audio assets; no generator runs on launch.

```sh
cmake --build build -j4
bash tests/wav-lab/prepare-fixture.sh
./build/sugarbee --console ./build/wav-lab/wav-lab.vcart
```

| Control | Action |
| --- | --- |
| Up/Down or W/S | Select mono/stereo one-shot or looping tone |
| A button / J | Play selected sound; repeat for overlap |
| B button / K | Stop all current playbacks |
| Left/Right or A/D | Set left/center/right pan on current and future playbacks |
| X button / U | Toggle quieter/louder volume on current and future playbacks |
| Start / Enter | Host pause/exit menu |

Launch is silent. The display counts active playbacks, including stopping voices.
At eight voices, another play displays `voices_full` and leaves the others alone.
Without audio output, loading still works and play displays `audio_unavailable`.
Stop wins simultaneous play/stop presses. Sounds remain loaded until teardown;
the lab does not attempt to unload assets while they are still stopping.

## Acceptance checks

1. Play each sound; verify expected pitch and stereo placement.
2. Trigger overlapping sounds; quickly press J repeatedly to reach eight voices.
   A full pool should refuse extra plays without a Guru or interrupting others.
3. Change volume/pan during playback; stop all, then play again.
4. Pause while playing; resume continues the frozen sounds.
5. Exit while playing, relaunch silently, and confirm playback still works.
6. Check Pong, the basic/musical Synth Labs, and Pac-Man. Exit to system returns 0.

`wav_lab_pack` generates and packages samples. `lua_wav_output` and
`lua_wav_unavailable` load the real cartridge, exercise input, API validation,
handles, overlaps, limits, unload, failure recovery, shutdown and relaunch.
Lua allocation-failure checks verify that handle preparation cannot leak a new
native asset or start playback. Invalid values are checked even after playback
ends. Dummy output is execution coverage, not listening validation.

Development verification (2026-11-04): Debug build completed without compiler
warnings; all 89 tests passed. AddressSanitizer/UndefinedBehaviorSanitizer runs
passed for output and unavailable-device cases. Leak detection was disabled
because the environment's tracing prevents LeakSanitizer operation; no leak-check
result is claimed. Lua allocation-failure injection is covered separately.

Desktop results (2026-11-04), user-reported on coastline: all samples play,
overlap works, and pause/resume, exit, and relaunch pass. Pi validation passed on 2026-11-06 at `604ae3b`, user-reported: all 89 tests
passed, all acceptance checks above passed, and Exit to system returned 0. See
[WAV API](../../docs/api/wav.md) for the full contract. Those results cover the prior one-shot checkpoint. Streaming remains
deferred; this checkpoint adds cartridge integration to the validated native
foundation without changing synthesis or playback gains.

## Whole-sound looping checkpoint

Looping is implemented. On 2026-11-06 the user confirmed all four coastline
check groups pass: sustained seamless looping, overlap/pan/volume/stop,
pause/resume/exit/silent relaunch, and original one-shots plus Pong/Synth Labs/
Pac-Man with Exit to system status 0. Pi validation remains pending.
Development verification (2026-11-06): build completed without compiler warnings;
all 89 tests passed, including expanded native and Lua WAV loop coverage.
Select the third sample, then play. Listen for at least ten seconds: the tone
should continue through several two-second boundaries without a repeating click
or gap. Trigger another copy to overlap, adjust volume/pan, and stop all. Repeat
with host pause/resume, exit while looping, and silent relaunch. Check the two
one-shot sounds still end normally and the eight-slot limit still refuses extra
plays. Repeat the existing gameplay and Exit to system checks.

The engine repeats decoded frames without crossfading. The generated loop uses
48 kHz to avoid resampling at its seam; arbitrary source files are not guaranteed
to loop seamlessly.
