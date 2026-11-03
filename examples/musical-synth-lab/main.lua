-- Mozart K.525, Allegro: first violin, bars 1-4, melody only, down one octave.
-- Public-domain source and transcription notes: README.md.
local synth = sugarbee.synth
local bpm = 120
-- MIDI note numbers are local score notation, not an engine MIDI interface.
-- 0 is a rest. Durations are quarter-note beats, including explicit rests.
local score = {
    {67,1}, {0,.5}, {62,.5}, {67,1}, {0,.5}, {62,.5},
    {67,.5}, {62,.5}, {67,.5}, {71,.5}, {74,1}, {0,1},
    {72,1}, {0,.5}, {69,.5}, {72,1}, {0,.5}, {69,.5},
    {72,.5}, {69,.5}, {66,.5}, {69,.5}, {62,1}, {0,1}
}
local total = 0
for _, event in ipairs(score) do
    event.start = total
    total = total + event[2]
    event.finish = total
end
local playing, sounding = false, false
local position, index = 0, 1
local status = "A / J: play"
local unavailable = false

local function release()
    if sounding then synth.note_off(1) end
    sounding = false
end

local function sound_current()
    local event = score[index]
    -- Short gaps articulate repeated notes. This is a lab performance choice.
    if event[1] ~= 0 and position < event.start + event[2] * .85 then
        local hz = 440 * 2 ^ ((event[1] - 69) / 12)
        local ok, reason = synth.note_on(1, hz)
        sounding = ok
        if not ok then
            unavailable = reason == "audio_unavailable"
            if not unavailable then
                status = reason or "Audio request failed"
                playing = false
            end
        end
    end
end

local function restart()
    release()
    position, index = 0, 1
    playing = true
    unavailable = false
    status = "Playing"
    sound_current()
end

function init()
    local ok, reason = synth.configure(1, {
        waveform = "triangle", attack = .008, decay = .04,
        sustain = .7, release = .025, volume = .7, pan = 0,
        filter = {bypass = true}
    })
    if not ok then status = reason or "Configuration failed" end
end

function update(dt)
    -- Stop wins simultaneous presses. Restart starts exactly at the first note.
    if sugarbee.btnp("b") then
        synth.stop(1)
        sounding, playing, unavailable = false, false, false
        position, index = 0, 1
        status = "Stopped"
        return
    end
    if sugarbee.btnp("a") then restart(); return end
    if not playing then return end
    position = math.min(total, position + dt * bpm / 60)
    local previous = index
    -- Carry elapsed time across boundaries; never discard the fractional beat.
    -- At most 24 entries. After a long update, skip elapsed notes rather than
    -- sending a burst of inaudible note-ons at the same instant.
    while index <= #score and position + 1e-9 >= score[index].finish do
        index = index + 1
    end
    if index ~= previous then
        release()
        if index > #score then
            playing = false
            position = total
            status = "Finished - A / J to replay"
            return
        end
        sound_current()
    end
    if sounding and position + 1e-9 >= score[index].start + score[index][2] * .85 then
        release()
    end
end

function draw()
    sugarbee.cls(24,30,40)
    sugarbee.print("MUSICAL SYNTH LAB",60,60,100,220,180,3)
    sugarbee.print("Mozart - Eine kleine Nachtmusik",60,140,255,255,255,2)
    sugarbee.print("Opening 4 bars / melody / one octave lower",60,190,220,220,220,2)
    sugarbee.print("120 BPM / triangle voice",60,240,220,220,220,2)
    local bar = math.min(4, math.floor((position + 1e-9) / 4) + 1)
    sugarbee.print("Bar " .. bar .. " / 4",60,300,255,220,120,3)
    sugarbee.print("A / J: play or restart    B / K: stop",60,380,220,220,220,2)
    sugarbee.print("START / ENTER: host pause / exit",60,430,220,220,220,2)
    sugarbee.print(unavailable and ("Audio unavailable - " .. status) or status,
                   60,510,255,220,120,2)
end

function shutdown()
    synth.stop(1)
    playing, sounding = false, false
end
