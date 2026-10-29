-- One counter, one explicitly requested save. Start belongs to the host.
local counter = 0
local maximum = 999999
local status = "No save yet"

local function load_counter()
    local data, code = sugarbee.load_save()
    if data == nil then
        if code == "not_found" then
            status = "No save yet - counter unchanged"
        else
            status = "Load failed: " .. code
        end
        return
    end

    -- Our format is decimal digits for an integer from 0 through 999999.
    -- Successfully reading bytes does not mean those bytes are valid game data.
    local value = tonumber(data)
    if #data == 0 or #data > 6 or not data:match("^%d+$")
        or value == nil or value > maximum then
        status = "Invalid counter data - counter unchanged"
        return
    end
    counter = value
    status = "Loaded"
end

function init()
    counter = 0
    load_counter()
end

function update(dt)
    -- btnp changes the counter once per press, not once per rendered frame.
    if sugarbee.btnp("left") then
        counter = math.max(0, counter - 1)
        status = "Counter changed - press A / J to save"
    elseif sugarbee.btnp("right") then
        counter = math.min(maximum, counter + 1)
        status = "Counter changed - press A / J to save"
    elseif sugarbee.btnp("a") then
        local ok, code = sugarbee.save(tostring(counter))
        if ok then
            status = "Saved"
        elseif code == "durability_uncertain" then
            status = "durability_uncertain - new save may be present"
        else
            status = "Save failed: " .. code
        end
    elseif sugarbee.btnp("b") then
        load_counter()
    end
end

function draw()
    sugarbee.cls(24, 30, 40)
    sugarbee.print("SAVE LAB", 60, 60, 100, 220, 180, 3)
    sugarbee.print("COUNTER: " .. counter, 60, 150, 255, 255, 255, 3)
    sugarbee.print("LEFT / RIGHT (or A / D keys): change counter", 60, 260, 220, 220, 220, 2)
    sugarbee.print("A button / J key: save     B button / K key: reload", 60, 310, 220, 220, 220, 2)
    sugarbee.print("START / ENTER: host pause and exit menu", 60, 360, 220, 220, 220, 2)
    sugarbee.print(status, 60, 460, 255, 220, 120, 2)
    sugarbee.print("Only A / J saves. Exiting does not save.", 60, 540, 220, 220, 220, 2)
end
