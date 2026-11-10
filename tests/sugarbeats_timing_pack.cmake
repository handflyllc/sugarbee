# Preserve the exported bytes; reuse the accepted Music Lab cartridge code.
file(SHA256 "${SOURCE_ROOT}/tests/sugarbeats-timing/song.json" digest)
if(NOT digest STREQUAL "5275d42154021993c4f45c3467c02d2f2e08e5ea8dea126d0fb9a32946cbe8b1")
    message(FATAL_ERROR "SugarBeats handoff asset checksum mismatch")
endif()
file(MAKE_DIRECTORY "${TEST_ROOT}/assets")
configure_file("${SOURCE_ROOT}/examples/music-lab/main.lua" "${TEST_ROOT}/main.lua" COPYONLY)
configure_file("${SOURCE_ROOT}/tests/sugarbeats-timing/song.json" "${TEST_ROOT}/assets/backing.json" COPYONLY)
execute_process(COMMAND "${PACKER}" -i "${TEST_ROOT}" -o "${CART}" -t "SugarBeats Timing Export"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SugarBeats export cartridge packaging failed")
endif()
