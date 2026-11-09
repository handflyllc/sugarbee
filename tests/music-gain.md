# Native music gain checkpoint

Internal API: `sb_music_sequence_set_gain(sequence, gain)`.

## Contract

- Accept finite 0..1 only while PLAYING or FINISHING. Idle/NULL/invalid requests
  return false without mutation. Caller serializes controls with rendering.
- Each successful start, including replay/loop start, initializes gain to 1.
- Apply gain to both channels after private-bank mixing, preserving instrument
  settings, balances and existing headroom. This control never amplifies above 1.
- A new target ramps linearly from the current gain over 480 rendered frames
  (10 ms). The first sample uses the current gain; after 480 intervals the state
  equals the target exactly. A repeated identical target does not extend a ramp.
- Mid-ramp retargeting starts from the current gain, without a discontinuity.
- Gain 0 mutes output but does not freeze events, loop position or voice lifecycle.
- Gain and its ramp live outside the initial-bank snapshot and survive every loop
  boundary. Stop envelopes and output gain run independently during FINISHING.
- Cleanup clears all gain state; no setting is retained while idle. Assets and
  their ownership are unchanged. There is no JSON gain field or Lua API.
- No host-pause, shared voices, or general runtime mixer integration is added.
  A host that stops rendering also stops ramp progress.

## Automated validation

`test_music_gain` compares output to an independently calculated gain schedule
applied to full-gain reference samples: mute/hold/unmute, mid-ramp retarget, many
loop boundaries and whole/single-frame buffers. It checks invalid refusal,
zero-frame rendering, idle behavior, stopping during a ramp, gain changes while
finishing, and default gain on replay. Existing parity/stop/loop tests remain.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build -R '^music_' --output-on-failure
./build/test_music_gain
ctest --test-dir build --output-on-failure
```

Desktop sequence size is 7352 bytes (32 bytes more than the prior layout);
bank snapshot stays 3640 bytes. Consumers must rebuild against the reviewed pin.
Desktop full suite: **99/99 passed** (105.45 seconds); all ten music-focused
checks passed. Six native music ASan/UBSan checks passed with leak detection
disabled under tracing. Debug build had no new warnings. Complete --gain host
passed with SDL dummy audio.
User confirmed all five desktop listening checks and exit status 0.
Pi listening and automated acceptance remain pending.

## Desktop and Pi listening

Start with low speaker volume:

```sh
./build/sbee-music-test --gain
echo $?
```

About ten seconds: four two-second loop passes, then a full-gain one-shot replay.
Gain changes to .25 around one second, 0 around two seconds and 1 around four
seconds. These are linear amplitude settings, not percentages of perceived loudness.
The diagnostic host issues controls under the audio-device lock; native tests
establish exact ramp timing independently of host scheduling.

Confirm:
1. Level decreases/restores cleanly, without unexpected clicks or bursts.
2. Music stays muted through the intervening loop boundary; no brief full-volume
   onset leaks through the mute.
3. Unmuted playback continues the expected rhythm; mute does not restart the song.
4. The final replay returns at normal full gain without an explicit gain command.
5. Completion is silent, exit status is 0, and relaunch behaves consistently.

Report tested hash, suite totals, listening results and exit status. After
acceptance, the follower task is bounded offline gain/parity/ownership checks.
No new preview controls or lifecycle layers belong to that follower checkpoint.
