# Preserve the exported bytes; reuse the accepted Music Lab cartridge code.
file(SHA256 "${SOURCE_ROOT}/tests/sugarbeats-export/song.json" digest)
if(NOT digest STREQUAL "962f729a5d5e598cb115653dba8d97db8f01f1f3c5f0527a45514f2f7b117c77")
    message(FATAL_ERROR "SugarBeats handoff asset checksum mismatch")
endif()
file(MAKE_DIRECTORY "${TEST_ROOT}/assets")
configure_file("${SOURCE_ROOT}/examples/music-lab/main.lua" "${TEST_ROOT}/main.lua" COPYONLY)
configure_file("${SOURCE_ROOT}/tests/sugarbeats-export/song.json" "${TEST_ROOT}/assets/backing.json" COPYONLY)
execute_process(COMMAND "${PACKER}" -i "${TEST_ROOT}" -o "${CART}" -t "SugarBeats First Export"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "SugarBeats export cartridge packaging failed")
endif()
