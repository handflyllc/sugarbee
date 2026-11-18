# Check both a nonzero exit and the intended diagnostic; unrelated failures fail.
file(MAKE_DIRECTORY "${TEST_ROOT}")
configure_file("${SOURCE_ROOT}/tests/smoke-runner/cart/song.json"
               "${TEST_ROOT}/song.json" COPYONLY)
function(check_failure source expected)
    file(WRITE "${TEST_ROOT}/main.lua" "${source}\n")
    execute_process(COMMAND "${LUA}" "${SOURCE_ROOT}/tests/cartridge_smoke.lua"
        "${TEST_ROOT}" 1 1 RESULT_VARIABLE result OUTPUT_VARIABLE output
        ERROR_VARIABLE error TIMEOUT 3)
    string(FIND "${error}" "${expected}" position)
    if(NOT "${result}" STREQUAL "1" OR position EQUAL -1)
        message(FATAL_ERROR "Unexpected runner result: ${result}\n${output}\n${error}")
    endif()
endfunction()
check_failure("sugarbee.audio.load('x.wav')" "unsupported API sugarbee.audio.load")
check_failure("sugarbee.unknown()" "unsupported API sugarbee.unknown")
check_failure("sugarbee.music.seek(0)" "unsupported API sugarbee.music.seek")
check_failure("sugarbee.music.load('song.json')" "music metadata required in scenario")
check_failure("sugarbee.music.load('../song.json')" "path segment '..' is rejected")
check_failure("sugarbee.music.load('/tmp/song.json')" "absolute paths are rejected")
check_failure("sugarbee.music.load('a:bad')" "invalid asset path")

check_failure("sugarbee.clip(0,0,-1,4)" "dimensions must be nonnegative")
check_failure("sugarbee.clip(2147483648,0,4,4)" "integer outside supported range")
check_failure("sugarbee.clip_reset(nil)" "expected no arguments")

check_failure("sugarbee.camera(0,2147483648)" "integer outside supported range")
check_failure("sugarbee.camera(0)" "expected x, y")
check_failure("sugarbee.camera(nil,0)" "integer expected")
check_failure("sugarbee.camera_reset(nil)" "expected no arguments")

check_failure("sugarbee.sprite_tint(0,0,256)" "channel must be in 0..255")
check_failure("sugarbee.sprite_tint(0,0,0,-1)" "channel must be in 0..255")
check_failure("sugarbee.sprite_tint(0,0)" "expected r, g, b")
check_failure("sugarbee.sprite_tint(0,0,0,nil)" "integer expected")
check_failure("sugarbee.sprite_tint_reset(nil)" "expected no arguments")

check_failure("sugarbee.line(0,0,0)" "expected x1, y1, x2, y2")
check_failure("sugarbee.line(2147483648,0,0,0)" "integer outside supported range")
check_failure("sugarbee.line(0,0,0,0,256)" "channel must be in 0..255")
check_failure("sugarbee.line(0,0,0,0,nil)" "integer expected")
