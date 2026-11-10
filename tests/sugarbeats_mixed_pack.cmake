# Preserve the exported bytes; reuse the accepted Music Lab cartridge code.
file(SHA256 "${SOURCE_ROOT}/tests/sugarbeats-mixed/song.json" digest)
if(NOT digest STREQUAL "bef13e743fbac100e409c1f19ebc0e65499a1cef43dc3e53faa97982f83b0377")
    message(FATAL_ERROR "SugarBeats handoff asset checksum mismatch")
endif()
file(MAKE_DIRECTORY "${TEST_ROOT}/assets")
configure_file("${SOURCE_ROOT}/examples/music-lab/main.lua" "${TEST_ROOT}/main.lua" COPYONLY)
configure_file("${SOURCE_ROOT}/tests/sugarbeats-mixed/song.json" "${TEST_ROOT}/assets/backing.json" COPYONLY)
execute_process(COMMAND "${PACKER}" -i "${TEST_ROOT}" -o "${CART}" -t "SugarBeats Mixed Export"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SugarBeats export cartridge packaging failed")
endif()
