# Direct pitch-control listening demo

One voice, initially silent, with a 0.6-second attack, 0.2-second decay, 0.6 sustain,
one-second release and volume 0.35. Pulse width is 50%; filter bypassed. Each
`update()` changes frequency directly without retriggering the envelope. No
pitch ramp or automatic smoothing is added. Use a comfortable listening volume.

```sh
./build/sbee-pack -i examples/pitch-control-demo -o build/pitch-control-demo.vcart \
  -t "Pitch Control" --uuid 6a193cf7-83ca-45b0-b827-45d0394eab13
./build/sugarbee build/pitch-control-demo.vcart
```

| Control | Action |
| --- | --- |
| A / J | Start or deliberately retrigger the note |
| B / K | Release over one second |
| X / U | Cycle stepped 220/440/880 Hz, 110–880 Hz sweep, A0–C8 sweep |
| Y / I | Cycle triangle, saw, 50% pulse (also while sounding) |
| Start / Enter | Host pause/exit menu |

The sweep takes four seconds up and four seconds down, using a logarithmic
frequency curve calculated by the game. The full-range sweep spans 27.5–4186.01
Hz; playback hardware may make its lowest and highest notes difficult to hear.
A pattern change intentionally jumps to that pattern's starting frequency.
Waveform changes use the already-existing short native waveform transition.

Desktop and Pi acceptance:

1. Start the stepped pattern. Hear one attack; pitch steps should not repeatedly
   swell as a retrigger would. J deliberately retriggers for comparison.
2. Try both sweeps on all three waveforms. Report clicks, buzzing or noticeable
   stepping, and whether it is objectionable. Different waveform brightness is
   expected; phase continuity alone is not a guarantee of artifact-free changes.
3. K releases; pitch changes have not prevented the fade. Pause/resume freezes
   and resumes normally. Exit silences the voice; relaunch starts silent/default.
4. Synth Lab, song-change and Pong still behave normally; normal host exit works.
5. Run the serial suite. Automated checks cover exact state preservation, range/
   argument errors, release edits, waveform transitions, music reservation,
   failure/unavailable output and all demo waveform/pattern combinations.

The user confirmed all desktop listening, lifecycle and regression checks passed
on 2026-11-20. Audible stepping and a digital character were noted but accepted
for this direct-control checkpoint. Pi acceptance is pending after a break.
A direct-versus-smoothed comparison remains a possible future investigation;
no smoothing implementation is authorized. See the
[API contract](../../docs/api/audio.md#direct-frequency-control).

Desktop automated validation on 2026-11-20: **193/193 serial tests passed**.
Automated rendering checks establish state and bounded samples, not listening quality.
