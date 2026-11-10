# Preserve the exported bytes; reuse the accepted Music Lab cartridge code.
file(SHA256 "${SOURCE_ROOT}/tests/sugarbeats-balanced/song.json" digest)
if(NOT digest STREQUAL "f3beb5dd5fb748abdf375d21800460ed6d9d0eff7a7eea0f05a7a445e8576f27")
    message(FATAL_ERROR "SugarBeats handoff asset checksum mismatch")
endif()
file(MAKE_DIRECTORY "${TEST_ROOT}/assets")
configure_file("${SOURCE_ROOT}/examples/music-lab/main.lua" "${TEST_ROOT}/main.lua" COPYONLY)
configure_file("${SOURCE_ROOT}/tests/sugarbeats-balanced/song.json" "${TEST_ROOT}/assets/backing.json" COPYONLY)
execute_process(COMMAND "${PACKER}" -i "${TEST_ROOT}" -o "${CART}" -t "SugarBeats Balanced Export"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SugarBeats export cartridge packaging failed")
endif()
