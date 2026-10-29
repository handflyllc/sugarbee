# Native diagnostic tone

From the repository root, start with a low speaker/headphone volume:

```sh
cmake --build build
./build/sbee-audio-test
```

Expect one centered 440 Hz sine tone lasting one second, with gentle entry and
exit, followed by silence and a normal exit. Both channels contain identical
samples. Peak amplitude is 0.05 (5% of digital full scale); actual loudness still
depends on your output volume. Run again to repeat. This executable requires no
cartridge, Lua, or window and uses the default SDL output device.

The tone has exactly 48,000 stereo frames at 48 kHz. Ten-ms linear fades are
included in that second, with first and final samples zero. Absolute frame
position preserves oscillator phase across callback boundaries. This sine is a
diagnostic signal, not an addition to the agreed synthesis waveform set.

The native request and active-state query briefly lock the SDL audio device so
callback state cannot be accessed concurrently. An active request is not
restarted. The callback performs no allocation, logging, or Lua access. Normal
SugarBee never requests a tone and continues to output silence.

The tool waits at most three seconds for generation, then supplies 250 ms of
silence before closing. This is not a device-drain or end-to-end latency
guarantee. Unavailable audio or generation timeout returns status 1; completion
returns 0. A completion message means samples were generated, not that a human
heard them. Driver-level blocking remains outside the application's timeout.

## Automated checks

```sh
ctest --test-dir build -R '^audio_' --output-on-failure
```

- `audio_tone_samples`: inactive silence, exact duration, 440 positive-going
  crossings, finite bounded amplitude, equal channels, fade bounds, silence
  afterward, and identical output across varying buffer sizes.
- `audio_tone_output`: runs the actual tool using the dummy driver, with a
  10-second external timeout. It does not play audible sound.
- Existing host tests also check request completion, refusal to retrigger an
  active tone, NULL/unavailable behavior, and unchanged host lifecycle.

All 42 automated tests pass on desktop. The user confirmed the tone sounds
clean through the intended output, with no clicks or distortion and normal exit.
Pi validation remains pending. Lua controls, general synthesis, WAV
playback, and repeated-note scheduling remain deferred.
