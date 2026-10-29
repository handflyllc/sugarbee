cmake_minimum_required(VERSION 3.16)
project(SugarBee VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SUGARBEE_BUILD_HOST "Build the SDL2/Lua SugarBee host" ON)
option(SUGARBEE_BUILD_PACKER "Build the .vcart packager" ON)

include(CTest)

function(sugarbee_warnings target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Pure-C fixed simulation scheduler is usable by every platform backend.
add_library(sugarbee_timing STATIC src/sb_timing.c)
target_include_directories(sugarbee_timing PUBLIC "${PROJECT_SOURCE_DIR}/include")
sugarbee_warnings(sugarbee_timing)

add_library(sugarbee_vcart STATIC src/vcart.c)
target_include_directories(sugarbee_vcart PUBLIC "${PROJECT_SOURCE_DIR}/include")
sugarbee_warnings(sugarbee_vcart)

# Bounded save storage is independent of SDL and Lua (Linux/POSIX backend).
add_library(sugarbee_save STATIC src/sb_save.c)
target_include_directories(sugarbee_save PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(sugarbee_save PRIVATE sugarbee_vcart)
sugarbee_warnings(sugarbee_save)

if(SUGARBEE_BUILD_PACKER)
    add_executable(sbee-pack tools/sbee-pack.c)
    target_link_libraries(sbee-pack PRIVATE sugarbee_vcart)
    sugarbee_warnings(sbee-pack)
endif()

if(SUGARBEE_BUILD_HOST)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    pkg_check_modules(LUA54 REQUIRED IMPORTED_TARGET lua5.4)
    pkg_check_modules(MINIZIP REQUIRED IMPORTED_TARGET minizip)
    find_package(ZLIB REQUIRED)
    find_package(PNG REQUIRED)

    # Cartridge reads and PNG decoding are independent of SDL and Lua.
    add_library(sugarbee_assets STATIC src/sb_assets.c src/sb_png.c)
    target_include_directories(sugarbee_assets PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(sugarbee_assets PUBLIC sugarbee_vcart PkgConfig::MINIZIP PNG::PNG)
    sugarbee_warnings(sugarbee_assets)

    # SDL is confined to the backend; games and their language adapters use
    # only include/sb_graphics.h for drawing services.
    add_library(sugarbee_graphics STATIC src/sb_graphics_sdl.c src/font8x8.c)
    target_include_directories(sugarbee_graphics PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(sugarbee_graphics PRIVATE PkgConfig::SDL2)
    sugarbee_warnings(sugarbee_graphics)

    add_library(sugarbee_input STATIC src/sb_input_sdl.c)
    target_include_directories(sugarbee_input PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(sugarbee_input PRIVATE PkgConfig::SDL2)
    sugarbee_warnings(sugarbee_input)

    add_executable(sugarbee src/main.c)
    target_link_libraries(sugarbee PRIVATE
        sugarbee_graphics
        sugarbee_input
        sugarbee_timing
        sugarbee_vcart
        sugarbee_assets
        PkgConfig::SDL2
        PkgConfig::LUA54
        PkgConfig::MINIZIP
        ZLIB::ZLIB)
    sugarbee_warnings(sugarbee)
endif()

if(BUILD_TESTING AND SUGARBEE_BUILD_HOST)
    # Include the host privately to test CLI and idle controls without an SDL window.
    add_executable(test_console_host tests/test_console_host.c)
    get_target_property(SUGARBEE_HOST_TEST_LIBRARIES sugarbee LINK_LIBRARIES)
    target_link_libraries(test_console_host PRIVATE ${SUGARBEE_HOST_TEST_LIBRARIES})
    sugarbee_warnings(test_console_host)
    add_test(NAME console_host_controls COMMAND test_console_host)

    add_executable(test_lua_libraries tests/test_lua_libraries.c)
    target_link_libraries(test_lua_libraries PRIVATE ${SUGARBEE_HOST_TEST_LIBRARIES})
    sugarbee_warnings(test_lua_libraries)
    add_test(NAME lua_supported_libraries COMMAND test_lua_libraries)

    add_executable(test_lua_close tests/test_lua_close.c)
    target_link_libraries(test_lua_close PRIVATE ${SUGARBEE_HOST_TEST_LIBRARIES})
    sugarbee_warnings(test_lua_close)
    foreach(close_case normal error runaway limit_then_runaway multiple_error multiple_runaway)
        add_test(NAME lua_close_${close_case} COMMAND test_lua_close ${close_case})
        # Enforced by CTest outside the process, even if Lua recovery hangs.
        set_tests_properties(lua_close_${close_case} PROPERTIES TIMEOUT 5)
    endforeach()

    add_executable(test_font tests/test_font.c)
    target_link_libraries(test_font PRIVATE sugarbee_graphics PkgConfig::SDL2)
    sugarbee_warnings(test_font)
    add_test(NAME font_smoke COMMAND test_font)
    set_tests_properties(font_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")

    add_executable(test_graphics tests/test_graphics.c)
    target_link_libraries(test_graphics PRIVATE sugarbee_graphics PkgConfig::SDL2)
    sugarbee_warnings(test_graphics)
    add_test(NAME graphics_smoke COMMAND test_graphics)
    set_tests_properties(graphics_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy")

    add_executable(test_texture tests/test_texture.c)
    target_link_libraries(test_texture PRIVATE sugarbee_graphics PkgConfig::SDL2)
    sugarbee_warnings(test_texture)
    add_test(NAME texture_smoke COMMAND test_texture)
    set_tests_properties(texture_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy")

    add_executable(test_png tests/test_png.c)
    target_link_libraries(test_png PRIVATE sugarbee_assets)
    sugarbee_warnings(test_png)
    add_test(NAME png_smoke COMMAND test_png
        "${PROJECT_SOURCE_DIR}/examples/asset-demo/assets/bee.png")
    add_test(NAME sprite_sheet_png_smoke COMMAND test_png
        "${PROJECT_SOURCE_DIR}/examples/sprite-sheet-demo/assets/bee-sheet.png" 192 32)

    add_executable(test_assets tests/test_assets.c)
    target_link_libraries(test_assets PRIVATE sugarbee_assets)
    sugarbee_warnings(test_assets)
    if(SUGARBEE_BUILD_PACKER)
        add_test(NAME asset_smoke
            COMMAND "${CMAKE_COMMAND}"
                "-DPACKER=$<TARGET_FILE:sbee-pack>"
                "-DREADER=$<TARGET_FILE:test_assets>"
                "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}/examples/asset-demo"
                "-DTEST_ROOT=${PROJECT_BINARY_DIR}/asset-smoke"
                "-P" "${PROJECT_SOURCE_DIR}/tests/asset_smoke.cmake")
    endif()

    add_executable(test_input tests/test_input.c)
    target_link_libraries(test_input PRIVATE sugarbee_input PkgConfig::SDL2)
    sugarbee_warnings(test_input)
    add_test(NAME input_smoke COMMAND test_input)
    set_tests_properties(input_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy")
endif()

if(BUILD_TESTING)
    add_executable(test_save tests/test_save.c)
    target_link_libraries(test_save PRIVATE sugarbee_save)
    sugarbee_warnings(test_save)
    add_test(NAME save_reader COMMAND test_save)
    set_tests_properties(save_reader PROPERTIES TIMEOUT 5)

    add_executable(test_save_write tests/test_save_write.c)
    target_link_libraries(test_save_write PRIVATE sugarbee_save)
    # Linux linker fault injection stays confined to this test executable.
    target_link_options(test_save_write PRIVATE
        "-Wl,--wrap=write" "-Wl,--wrap=fsync" "-Wl,--wrap=close"
        "-Wl,--wrap=renameat" "-Wl,--wrap=unlinkat")
    sugarbee_warnings(test_save_write)
    add_test(NAME save_writer COMMAND test_save_write)
    set_tests_properties(save_writer PROPERTIES TIMEOUT 15)

    add_executable(test_save_directory tests/test_save_directory.c)
    target_link_libraries(test_save_directory PRIVATE sugarbee_save)
    target_link_options(test_save_directory PRIVATE
        "-Wl,--wrap=mkdirat" "-Wl,--wrap=openat" "-Wl,--wrap=fsync" "-Wl,--wrap=close")
    sugarbee_warnings(test_save_directory)
    add_test(NAME save_directory COMMAND test_save_directory)
    set_tests_properties(save_directory PROPERTIES TIMEOUT 15)

    add_executable(test_save_path tests/test_save_path.c)
    target_link_libraries(test_save_path PRIVATE sugarbee_save)
    target_link_options(test_save_path PRIVATE
        "-Wl,--wrap=mkdirat" "-Wl,--wrap=openat" "-Wl,--wrap=fsync"
        "-Wl,--wrap=close" "-Wl,--wrap=write" "-Wl,--wrap=renameat")
    sugarbee_warnings(test_save_path)
    add_test(NAME save_path COMMAND test_save_path)
    set_tests_properties(save_path PROPERTIES TIMEOUT 15)


    add_executable(test_timing tests/test_timing.c)
    target_link_libraries(test_timing PRIVATE sugarbee_timing)
    sugarbee_warnings(test_timing)
    add_test(NAME timing_smoke COMMAND test_timing)

    add_executable(test_vcart tests/test_vcart.c)
    target_link_libraries(test_vcart PRIVATE sugarbee_vcart)
    sugarbee_warnings(test_vcart)
    add_test(NAME cartridge_header COMMAND test_vcart)

    if(SUGARBEE_BUILD_PACKER)
        add_test(NAME packager_smoke
            COMMAND "${CMAKE_COMMAND}"
                "-DPACKER=$<TARGET_FILE:sbee-pack>"
                "-DTEST_ROOT=${PROJECT_BINARY_DIR}/packager-smoke"
                "-P" "${PROJECT_SOURCE_DIR}/tests/packager_smoke.cmake")
    endif()

    # Run each example cartridge headlessly against a stand-in sugarbee API
    # (tests/cartridge_smoke.lua). Needs only the Lua 5.4 interpreter, not
    # SDL or the host; skip these tests unless the interpreter reports Lua 5.4.
    find_program(LUA54_INTERPRETER NAMES lua5.4 lua)
    set(LUA54_INTERPRETER_COMPATIBLE FALSE)
    if(LUA54_INTERPRETER)
        execute_process(
            COMMAND "${LUA54_INTERPRETER}" -e "io.write(_VERSION)"
            RESULT_VARIABLE LUA54_VERSION_RESULT
            OUTPUT_VARIABLE LUA54_VERSION_OUTPUT
            ERROR_VARIABLE LUA54_VERSION_ERROR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            TIMEOUT 5)
        if("${LUA54_VERSION_RESULT}" STREQUAL "0")
            if("${LUA54_VERSION_OUTPUT}" STREQUAL "Lua 5.4")
                set(LUA54_INTERPRETER_COMPATIBLE TRUE)
                message(STATUS "Lua 5.4 interpreter: ${LUA54_INTERPRETER}; cartridge_smoke tests enabled")
            else()
                message(STATUS "Interpreter '${LUA54_INTERPRETER}' reports '${LUA54_VERSION_OUTPUT}'; Lua 5.4 required; cartridge_smoke tests disabled")
            endif()
        else()
            message(STATUS "Cannot query Lua interpreter '${LUA54_INTERPRETER}' (${LUA54_VERSION_RESULT}): ${LUA54_VERSION_ERROR}; cartridge_smoke tests disabled")
        endif()
    else()
        message(STATUS "Lua interpreter not found; Lua 5.4 required; cartridge_smoke tests disabled")
    endif()

    if(LUA54_INTERPRETER_COMPATIBLE)
        foreach(cartridge IN ITEMS graphics-demo moving-box sprite-demo
                asset-demo sprite-sheet-demo pong-lab invaders-lab pacman-demo)
            add_test(NAME cartridge_smoke_${cartridge}
                COMMAND "${LUA54_INTERPRETER}"
                    "${PROJECT_SOURCE_DIR}/tests/cartridge_smoke.lua"
                    "${PROJECT_SOURCE_DIR}/examples/${cartridge}" 3000)
        endforeach()
    endif()
endif()
