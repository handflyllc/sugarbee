-- Cases: long_message, long_word, vertical_overflow
local test_case = "long_message"

function init()
    if test_case == "long_message" then
        error(string.rep("Long diagnostic test. ", 30)
              .. "END OF MESSAGE")
    elseif test_case == "long_word" then
        error(string.rep("X", 180) .. " END")
    elseif test_case == "vertical_overflow" then
        error(string.rep("Row\n", 25) .. "END OF MESSAGE")
    else
        error("Unknown Guru text test case: " .. test_case)
    end
end
