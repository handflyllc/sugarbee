-- Deterministic cartridge-logic checks; native storage is tested separately.
local source = assert(arg[1])
local stored, load_error, save_error, delete_error
local deletions = 0
local pressed = {}
local lines = {}
local saves = 0
sugarbee = {
    cls = function() end,
    print = function(text) lines[#lines + 1] = text end,
    btnp = function(button) return pressed[button] == true end,
    load_save = function()
        if load_error then return nil, load_error end
        if stored == nil then return nil, "not_found" end
        return stored, nil
    end,
    delete_save = function()
        deletions = deletions + 1
        if delete_error then return nil, delete_error end
        stored = nil
        return true, nil
    end,
    save = function(data)
        assert(type(data) == "string")
        saves = saves + 1
        if save_error then return nil, save_error end
        stored = data
        return true, nil
    end,
}
local function screen(counter, message)
    lines = {}
    draw()
    assert(lines[2] == "COUNTER: " .. counter, table.concat(lines, "\n"))
    assert(lines[7] == message, table.concat(lines, "\n"))
end
local function press(button)
    pressed = {[button] = true}
    update(1/60)
    pressed = {}
    update(1/60)
end
local function launch()
    dofile(source)
    init()
end
launch()
screen(0, "No save yet - counter unchanged")
press("right"); press("right"); press("right")
assert(saves == 0)
press("a"); screen(3, "Saved"); assert(stored == "3")
press("right"); press("b"); screen(3, "Loaded")
press("right"); launch(); screen(3, "Loaded"); assert(saves == 1)
press("left"); press("left"); press("left"); press("left")
press("a"); press("b"); screen(0, "Loaded"); assert(stored == "0")
stored = "999999"; press("b"); press("right")
press("a"); assert(stored == "999999")
for _, invalid in ipairs({"", "-1", "1.5", "1e2", " 3", "1000000", "nan", "3\0"}) do
    stored = invalid
    press("b"); screen(999999, "Invalid counter data - counter unchanged")
end
stored = nil; press("b"); screen(999999, "No save yet - counter unchanged")
for _, code in ipairs({"io_error", "too_large"}) do
    load_error = code; press("b"); screen(999999, "Load failed: " .. code)
    save_error = code; press("a"); screen(999999, "Save failed: " .. code)
end
load_error = nil
save_error = "durability_uncertain"; press("a")
screen(999999, "durability_uncertain - new save may be present")
save_error = nil
press("a"); assert(stored == "999999")
press("x"); screen(999999, "Erase save? X / U confirms; B / K cancels")
assert(deletions == 0)
press("b"); screen(999999, "Delete cancelled"); assert(stored == "999999")
press("x"); press("x"); screen(999999, "Save deleted - counter unchanged")
assert(deletions == 1 and stored == nil)
press("b"); screen(999999, "No save yet - counter unchanged")
press("x"); press("x"); assert(deletions == 2)
press("a"); assert(stored == "999999")
for _,code in ipairs({"io_error", "durability_uncertain"}) do
    delete_error = code
    press("x"); press("x")
    screen(999999, code == "io_error" and "Delete failed: io_error" or "durability_uncertain - save may be gone")
end
delete_error = nil
local before = saves
for _ = 1, 120 do update(1/60); draw() end
assert(saves == before and shutdown == nil)
print("Save Lab behavior checks passed")
