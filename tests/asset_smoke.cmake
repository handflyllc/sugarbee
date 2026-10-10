if(NOT PACKER OR NOT READER OR NOT SOURCE_ROOT OR NOT TEST_ROOT)
    message(FATAL_ERROR "PACKER, READER, SOURCE_ROOT, and TEST_ROOT are required")
endif()
file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
set(cart "${TEST_ROOT}/asset-demo.vcart")
execute_process(COMMAND "${PACKER}" -i "${SOURCE_ROOT}" -o "${cart}"
    -t "Asset Smoke" --uuid "123e4567-e89b-42d3-a456-426614174000"
    RESULT_VARIABLE packaged OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT packaged EQUAL 0)
    message(FATAL_ERROR "Failed to package asset fixture: ${output} ${error}")
endif()
execute_process(COMMAND "${READER}" "${cart}"
    RESULT_VARIABLE tested OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT tested EQUAL 0)
    message(FATAL_ERROR "Asset smoke test failed: ${output} ${error}")
endif()
