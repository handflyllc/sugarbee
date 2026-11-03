# Synth Lab

From the repository root:

```sh
cmake --build build -j4
bash tests/synth-lab/prepare-fixture.sh
./build/sugarbee --console ./build/synth-lab/synth-lab.vcart
```

| Control | Action |
| --- | --- |
| Left/Right or A/D keys | Select triangle, saw, pulse, or noise |
| Up/Down or W/S keys | Select 110, 220, 440, 660, or 880 Hz |
| Hold A button / J key | Play; releasing starts the envelope release |
| B button / K key | Toggle the 700 Hz low-pass filter |
| Start / Enter | Host pause/exit menu |

Changing waveform while held preserves the note; changing pitch deliberately
retriggers it. Noise has no pitch. Requests rejected during a waveform transition
show `audio_rejected` and leave the selection unchanged. With no device, the lab
still runs and shows `audio_unavailable` when trying to play.

## Desktop and Pi acceptance

1. Launch silently. Hold/release J and check the tone starts and releases.
2. Compare all four waveforms; noise should be clearly different. Try pitch
   endpoints and change waveform while sounding.
3. Toggle the filter during saw/pulse/noise: it should soften the sound.
4. Pause during a held note: audio fades to silence. Resume uses the frozen
   state. Host input suppression may release the note on the first resumed
   update; release and press J again to play normally.
5. Exit while sounding, then relaunch: no stuck notes and default settings.
6. Check Pong/Pac-Man, then Exit to system and `echo $?` (expect 0).

Run `ctest --test-dir build --output-on-failure` as well. Dummy-device tests
verify execution, not audible quality. Development Debug build completed without
compiler warnings on 2026-11-03; all 75 automated tests passed.

Desktop acceptance (2026-11-03), user-reported for the implementation based on
`240a4f0`: all sound checks passed, with successful exit and relaunch. Pi
validation is pending; record its results against the implementation commit.

The lab currently uses one voice. Regression tests cover independent voices and
API rejection paths. A musical lab with a short public-domain passage is the
accepted follow-up; this cartridge is the basic controls checkpoint.
