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
        PkgConfig::SDL2
        PkgConfig::LUA54
        PkgConfig::MINIZIP
        ZLIB::ZLIB)
    sugarbee_warnings(sugarbee)
endif()

if(BUILD_TESTING AND SUGARBEE_BUILD_HOST)
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

    add_executable(test_input tests/test_input.c)
    target_link_libraries(test_input PRIVATE sugarbee_input PkgConfig::SDL2)
    sugarbee_warnings(test_input)
    add_test(NAME input_smoke COMMAND test_input)
    set_tests_properties(input_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy")
endif()

if(BUILD_TESTING)
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
endif()
