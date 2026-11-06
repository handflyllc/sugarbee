-- Generated assets are packed by tests/wav-lab/prepare-fixture.sh.
local audio=sugarbee.audio
local sounds={}
local names={"Mono chime", "Stereo: low left / high right", "Loop: steady mono tone"}
local selected, volume, pan = 1, .8, 0
local playing={}
local status="A / J: play"
function init()
    for i,path in ipairs({"assets/mono.wav","assets/stereo.wav","assets/loop.wav"}) do
        local sound,why=audio.load(path)
        if not sound then error("WAV load failed: " .. why) end
        sounds[i]=sound
    end
end
function update(dt)
    for i=#playing,1,-1 do
        if not audio.is_playing(playing[i]) then table.remove(playing,i) end
    end
    if sugarbee.btnp("up") then selected=(selected-2)%#sounds+1 end
    if sugarbee.btnp("down") then selected=selected%#sounds+1 end
    local newpan=pan
    if sugarbee.btnp("left") then newpan=math.max(-1,pan-1) end
    if sugarbee.btnp("right") then newpan=math.min(1,pan+1) end
    if newpan~=pan then
        pan=newpan
        for _,handle in ipairs(playing) do audio.set_pan(handle,pan) end
    end
    if sugarbee.btnp("x") then
        volume=volume==.8 and .3 or .8
        for _,handle in ipairs(playing) do audio.set_volume(handle,volume) end
    end
    if sugarbee.btnp("b") then
        for _,handle in ipairs(playing) do audio.stop(handle) end
        status="Stopping"
    elseif sugarbee.btnp("a") then
        local handle,why=audio.play(sounds[selected],{volume=volume,pan=pan,loop=selected==3})
        if handle then
            playing[#playing+1]=handle; status="Playing"
        else status=why end
    end
end
function draw()
    sugarbee.cls(24,30,40)
    sugarbee.print("WAV LAB",60,60,100,220,180,3)
    sugarbee.print(names[selected],60,140,255,255,255,2)
    sugarbee.print("Active: " .. #playing .. " / 8",60,200,255,220,120,2)
    sugarbee.print("Pan: " .. pan .. "    Volume: " .. volume,60,250,220,220,220,2)
    sugarbee.print("UP / DOWN (W / S): choose sample",60,310,220,220,220,2)
    sugarbee.print("A / J: play (repeat for overlap)    B / K: stop all",60,350,220,220,220,2)
    sugarbee.print("LEFT / RIGHT (A / D): pan    X / U: volume",60,390,220,220,220,2)
    sugarbee.print("START / ENTER: host pause / exit",60,430,220,220,220,2)
    sugarbee.print(status,60,510,255,220,120,2)
end
function shutdown()
    for _,handle in ipairs(playing) do audio.stop(handle) end
    -- Stopping voices still own their assets. Native teardown releases them.
end
