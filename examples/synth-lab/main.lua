-- Basic control lab. A musical passage is a separate follow-up.
local synth = sugarbee.synth
local waves = {"triangle", "saw", "pulse", "noise"}
local pitches = {110, 220, 440, 660, 880}
local wave, pitch = 1, 3
local filtered, held = false, false
local status = "Hold A / J to play"

local function report(ok, why)
    status = ok and "Ready" or (why or "Audio request failed")
    return ok
end

local function play()
    if waves[wave] == "noise" then
        report(synth.note_on(1))
    else
        report(synth.note_on(1, pitches[pitch]))
    end
end

function init()
    report(synth.configure(1, {
        waveform = waves[wave], attack = 0.02, decay = 0.1,
        sustain = 0.7, release = 0.2, volume = 0.7, pan = 0,
        pulse_width = 0.3, filter = {cutoff = 700, bypass = true}
    }))
end

function update(dt)
    local next_wave = wave
    if sugarbee.btnp("left") then next_wave = (wave - 2) % #waves + 1 end
    if sugarbee.btnp("right") then next_wave = wave % #waves + 1 end
    if next_wave ~= wave then
        if report(synth.configure(1, {waveform = waves[next_wave]})) then
            wave = next_wave
        end
    end
    local next_pitch = pitch
    if sugarbee.btnp("up") then next_pitch = math.min(#pitches, pitch + 1) end
    if sugarbee.btnp("down") then next_pitch = math.max(1, pitch - 1) end
    if next_pitch ~= pitch then
        pitch = next_pitch
        if held then play() end -- Deliberate note retrigger, not a pitch-only edit.
    end
    if sugarbee.btnp("b") then
        if report(synth.configure(1, {filter = {bypass = filtered}})) then
            filtered = not filtered
        end
    end
    local down = sugarbee.btn("a")
    if down and not held then play() end
    if held and not down then report(synth.note_off(1)) end
    held = down
end

function draw()
    sugarbee.cls(24, 30, 40)
    sugarbee.print("SYNTH LAB", 60, 60, 100, 220, 180, 3)
    sugarbee.print("VOICE 1: " .. waves[wave], 60, 140, 255, 255, 255, 3)
    local pitch_text = waves[wave] == "noise" and "NOISE: no pitch" or (pitches[pitch] .. " Hz")
    sugarbee.print(pitch_text, 60, 200, 255, 255, 255, 2)
    sugarbee.print("LEFT / RIGHT (A / D): waveform", 60, 270, 220, 220, 220, 2)
    sugarbee.print("UP / DOWN (W / S): pitch; retriggers held note", 60, 310, 220, 220, 220, 2)
    sugarbee.print("Hold A / J: play; release: note off", 60, 350, 220, 220, 220, 2)
    sugarbee.print("B / K: low-pass " .. (filtered and "ON" or "OFF"), 60, 390, 220, 220, 220, 2)
    sugarbee.print("START / ENTER: pause / exit", 60, 430, 220, 220, 220, 2)
    sugarbee.print(status, 60, 500, 255, 220, 120, 2)
end

function shutdown()
    synth.stop(1)
end
