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

# Standalone sample-timed envelope; independent of SDL, Lua, and oscillators.
add_library(sugarbee_envelope STATIC src/sb_audio_envelope.c)
sugarbee_warnings(sugarbee_envelope)
if(BUILD_TESTING)
    add_executable(test_audio_envelope tests/test_audio_envelope.c)
    target_link_libraries(test_audio_envelope PRIVATE sugarbee_envelope)
    sugarbee_warnings(test_audio_envelope)
    add_test(NAME audio_envelope COMMAND test_audio_envelope)
    set_tests_properties(audio_envelope PROPERTIES TIMEOUT 5)
endif()

# Pulse oscillator with width smoothing and explicit DC conditioning.
add_library(sugarbee_pulse STATIC src/sb_audio_pulse.c)
sugarbee_warnings(sugarbee_pulse)
if(BUILD_TESTING)
    add_executable(test_audio_pulse_dc tests/test_audio_pulse_dc.c)
    target_link_libraries(test_audio_pulse_dc PRIVATE sugarbee_pulse m)
    sugarbee_warnings(test_audio_pulse_dc)
    add_test(NAME audio_pulse_dc COMMAND test_audio_pulse_dc)
    set_tests_properties(audio_pulse_dc PROPERTIES TIMEOUT 10)
    add_executable(test_audio_pulse_width tests/test_audio_pulse_width.c)
    target_link_libraries(test_audio_pulse_width PRIVATE sugarbee_pulse m)
    sugarbee_warnings(test_audio_pulse_width)
    add_test(NAME audio_pulse_width COMMAND test_audio_pulse_width)
    set_tests_properties(audio_pulse_width PROPERTIES TIMEOUT 10)
    add_executable(test_audio_pulse tests/test_audio_pulse.c)
    target_link_libraries(test_audio_pulse PRIVATE sugarbee_pulse m)
    sugarbee_warnings(test_audio_pulse)
    add_test(NAME audio_pulse COMMAND test_audio_pulse)
    set_tests_properties(audio_pulse PROPERTIES TIMEOUT 10)
endif()

# Independent saw oscillator, also used by the native bank.
add_library(sugarbee_saw STATIC src/sb_audio_saw.c)
sugarbee_warnings(sugarbee_saw)
if(BUILD_TESTING)
    add_executable(test_audio_saw tests/test_audio_saw.c)
    target_link_libraries(test_audio_saw PRIVATE sugarbee_saw m)
    sugarbee_warnings(test_audio_saw)
    add_test(NAME audio_saw COMMAND test_audio_saw)
    set_tests_properties(audio_saw PROPERTIES TIMEOUT 10)
endif()

# Independent deterministic noise source, also used by the native bank.
add_library(sugarbee_noise STATIC src/sb_audio_noise.c)
sugarbee_warnings(sugarbee_noise)
if(BUILD_TESTING)
    add_executable(test_audio_noise tests/test_audio_noise.c)
    target_link_libraries(test_audio_noise PRIVATE sugarbee_noise m)
    sugarbee_warnings(test_audio_noise)
    add_test(NAME audio_noise COMMAND test_audio_noise)
    set_tests_properties(audio_noise PROPERTIES TIMEOUT 5)
endif()

# Pure-C voice rendering and nine-voice mixing, usable without the host.
add_library(sugarbee_synth STATIC src/sb_audio_triangle.c src/sb_audio_bank.c src/sb_audio_filter.c)
target_include_directories(sugarbee_synth PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(sugarbee_synth PRIVATE sugarbee_envelope sugarbee_noise sugarbee_saw sugarbee_pulse m)
sugarbee_warnings(sugarbee_synth)
if(BUILD_TESTING)
    add_executable(test_audio_filter tests/test_audio_filter.c)
    target_link_libraries(test_audio_filter PRIVATE sugarbee_synth)
    sugarbee_warnings(test_audio_filter)
    add_test(NAME audio_filter COMMAND test_audio_filter)
    set_tests_properties(audio_filter PROPERTIES TIMEOUT 10)

    add_executable(test_audio_pitch_range tests/test_audio_pitch_range.c)
    target_link_libraries(test_audio_pitch_range PRIVATE sugarbee_synth)
    sugarbee_warnings(test_audio_pitch_range)
    add_test(NAME audio_pitch_range COMMAND test_audio_pitch_range)
    set_tests_properties(audio_pitch_range PROPERTIES TIMEOUT 10)

    add_executable(test_audio_noise_switch tests/test_audio_noise_switch.c)
    target_link_libraries(test_audio_noise_switch PRIVATE sugarbee_synth)
    sugarbee_warnings(test_audio_noise_switch)
    add_test(NAME audio_noise_switch COMMAND test_audio_noise_switch)
    set_tests_properties(audio_noise_switch PROPERTIES TIMEOUT 5)
    add_executable(test_audio_waveform tests/test_audio_waveform.c)
    target_link_libraries(test_audio_waveform PRIVATE sugarbee_synth)
    sugarbee_warnings(test_audio_waveform)
    add_test(NAME audio_waveform COMMAND test_audio_waveform)
    set_tests_properties(audio_waveform PROPERTIES TIMEOUT 5)
    add_executable(test_audio_pulse_bank tests/test_audio_pulse_bank.c)
    target_link_libraries(test_audio_pulse_bank PRIVATE sugarbee_synth)
    sugarbee_warnings(test_audio_pulse_bank)
    add_test(NAME audio_pulse_bank COMMAND test_audio_pulse_bank)
    set_tests_properties(audio_pulse_bank PROPERTIES TIMEOUT 5)
    add_executable(test_audio_saw_bank tests/test_audio_saw_bank.c)
    target_link_libraries(test_audio_saw_bank PRIVATE sugarbee_synth)
    sugarbee_warnings(test_audio_saw_bank)
    add_test(NAME audio_saw_bank COMMAND test_audio_saw_bank)
    set_tests_properties(audio_saw_bank PROPERTIES TIMEOUT 5)
    add_executable(test_audio_noise_bank tests/test_audio_noise_bank.c)
    target_link_libraries(test_audio_noise_bank PRIVATE sugarbee_synth)
    sugarbee_warnings(test_audio_noise_bank)
    add_test(NAME audio_noise_bank COMMAND test_audio_noise_bank)
    set_tests_properties(audio_noise_bank PROPERTIES TIMEOUT 5)
    add_executable(test_audio_bank tests/test_audio_bank.c)
    target_link_libraries(test_audio_bank PRIVATE sugarbee_synth)
    sugarbee_warnings(test_audio_bank)
    add_test(NAME audio_bank COMMAND test_audio_bank)
    set_tests_properties(audio_bank PROPERTIES TIMEOUT 10)
endif()

add_library(sugarbee_music_sequence STATIC src/sb_music_sequence.c)
target_link_libraries(sugarbee_music_sequence PUBLIC sugarbee_synth)
sugarbee_warnings(sugarbee_music_sequence)
if(BUILD_TESTING)
    add_executable(test_music_sequence tests/test_music_sequence.c)
    target_link_libraries(test_music_sequence PRIVATE sugarbee_music_sequence)
    sugarbee_warnings(test_music_sequence)
    add_test(NAME music_sequence COMMAND test_music_sequence)
    set_tests_properties(music_sequence PROPERTIES TIMEOUT 10)
endif()

# Pinned parser stays private to the memory-only music loader.
add_library(sugarbee_yyjson STATIC third_party/yyjson/yyjson.c)
target_include_directories(sugarbee_yyjson PUBLIC "${PROJECT_SOURCE_DIR}/third_party/yyjson")
target_compile_definitions(sugarbee_yyjson PRIVATE YYJSON_DISABLE_WRITER=1 YYJSON_DISABLE_UTILS=1)
add_library(sugarbee_music_load STATIC src/sb_music_load.c)
target_link_libraries(sugarbee_music_load PUBLIC sugarbee_music_sequence PRIVATE sugarbee_yyjson)
sugarbee_warnings(sugarbee_music_load)
if(BUILD_TESTING)
    add_executable(test_music_load tests/test_music_load.c)
    target_link_libraries(test_music_load PRIVATE sugarbee_music_load)
    target_link_options(test_music_load PRIVATE "-Wl,--wrap=malloc" "-Wl,--wrap=free")
    target_compile_definitions(test_music_load PRIVATE MUSIC_FIXTURES="${PROJECT_SOURCE_DIR}/tests/music-load")
    sugarbee_warnings(test_music_load)
    add_executable(test_music_loop_eligibility tests/test_music_loop_eligibility.c)
    target_link_libraries(test_music_loop_eligibility PRIVATE sugarbee_music_load)
    sugarbee_warnings(test_music_loop_eligibility)
    add_test(NAME music_loop_eligibility COMMAND test_music_loop_eligibility)
    set_tests_properties(music_loop_eligibility PROPERTIES TIMEOUT 10)
    add_test(NAME music_load COMMAND test_music_load)
    set_tests_properties(music_load PROPERTIES TIMEOUT 20)
endif()

add_library(sugarbee_wav STATIC src/sb_wav.c)
target_include_directories(sugarbee_wav PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(sugarbee_wav PRIVATE m)
sugarbee_warnings(sugarbee_wav)
if(BUILD_TESTING)
    add_executable(test_wav tests/test_wav.c)
    target_link_libraries(test_wav PRIVATE sugarbee_wav m)
    target_link_options(test_wav PRIVATE "-Wl,--wrap=malloc")
    sugarbee_warnings(test_wav)
    add_test(NAME wav_core COMMAND test_wav)
    set_tests_properties(wav_core PROPERTIES TIMEOUT 10)
endif()

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

    add_library(sugarbee_audio STATIC src/sb_audio_sdl.c src/sb_audio_tone.c)
    target_include_directories(sugarbee_audio PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(sugarbee_audio PRIVATE sugarbee_synth sugarbee_wav PkgConfig::SDL2 m)
    sugarbee_warnings(sugarbee_audio)

    add_executable(generate_wav_lab tests/wav-lab/generate.c)
    target_link_libraries(generate_wav_lab PRIVATE m)
    sugarbee_warnings(generate_wav_lab)

    add_executable(sbee-wav-test tools/sbee-wav-test.c)
    target_link_libraries(sbee-wav-test PRIVATE sugarbee_audio PkgConfig::SDL2 m)
    sugarbee_warnings(sbee-wav-test)

    add_executable(sbee-session-test tools/sbee-session-test.c)
    target_link_libraries(sbee-session-test PRIVATE sugarbee_audio PkgConfig::SDL2 m)
    sugarbee_warnings(sbee-session-test)
    add_executable(sbee-filter-test tools/sbee-filter-test.c)
    target_link_libraries(sbee-filter-test PRIVATE sugarbee_audio PkgConfig::SDL2 m)
    sugarbee_warnings(sbee-filter-test)

    add_executable(sbee-pitch-test tools/sbee-pitch-test.c)
    target_link_libraries(sbee-pitch-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-pitch-test)

    add_executable(sbee-noise-switch-test tools/sbee-noise-switch-test.c)
    target_link_libraries(sbee-noise-switch-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-noise-switch-test)
    add_executable(sbee-waveform-test tools/sbee-waveform-test.c)
    target_link_libraries(sbee-waveform-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-waveform-test)
    add_executable(sbee-pulse-test tools/sbee-pulse-test.c)
    target_link_libraries(sbee-pulse-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-pulse-test)
    add_executable(sbee-saw-test tools/sbee-saw-test.c)
    target_link_libraries(sbee-saw-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-saw-test)

    add_executable(sbee-noise-test tools/sbee-noise-test.c)
    target_link_libraries(sbee-noise-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-noise-test)

    add_executable(sbee-bank-test tools/sbee-bank-test.c)
    target_link_libraries(sbee-bank-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-bank-test)

    add_executable(sbee-music-test tools/sbee-music-test.c)
    target_link_libraries(sbee-music-test PRIVATE sugarbee_music_sequence PkgConfig::SDL2)
    sugarbee_warnings(sbee-music-test)
    if(BUILD_TESTING)
        add_test(NAME music_sequence_sdl COMMAND sbee-music-test --smoke)
        set_tests_properties(music_sequence_sdl PROPERTIES
            ENVIRONMENT "SDL_AUDIODRIVER=dummy" TIMEOUT 10)
    endif()

    add_executable(sbee-pan-test tools/sbee-pan-test.c)
    target_link_libraries(sbee-pan-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-pan-test)

    add_executable(sbee-triangle-test tools/sbee-triangle-test.c)
    target_link_libraries(sbee-triangle-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-triangle-test)

    add_executable(sbee-audio-test tools/sbee-audio-test.c)
    target_link_libraries(sbee-audio-test PRIVATE sugarbee_audio PkgConfig::SDL2)
    sugarbee_warnings(sbee-audio-test)

    add_library(sugarbee_lua_synth STATIC src/sb_lua_synth.c)
    target_include_directories(sugarbee_lua_synth PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(sugarbee_lua_synth PUBLIC sugarbee_audio PkgConfig::LUA54)
    sugarbee_warnings(sugarbee_lua_synth)

    add_library(sugarbee_lua_audio STATIC src/sb_lua_audio.c)
    target_include_directories(sugarbee_lua_audio PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(sugarbee_lua_audio PUBLIC sugarbee_audio sugarbee_assets sugarbee_wav PkgConfig::LUA54)
    sugarbee_warnings(sugarbee_lua_audio)

    add_executable(sugarbee src/main.c)
    target_link_libraries(sugarbee PRIVATE
        sugarbee_save
        sugarbee_lua_synth
        sugarbee_lua_audio
        sugarbee_audio
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

    add_test(NAME wav_output COMMAND sbee-wav-test)
    set_tests_properties(wav_output PROPERTIES TIMEOUT 15 ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    add_executable(test_wav_sdl tests/test_wav_sdl.c src/sb_audio_tone.c)
    target_link_libraries(test_wav_sdl PRIVATE sugarbee_synth sugarbee_wav PkgConfig::SDL2 m)
    sugarbee_warnings(test_wav_sdl)
    add_test(NAME wav_sdl COMMAND test_wav_sdl)
    set_tests_properties(wav_sdl PROPERTIES TIMEOUT 5 ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    add_executable(test_audio_session tests/test_audio_session.c src/sb_audio_tone.c)
    target_include_directories(test_audio_session PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(test_audio_session PRIVATE sugarbee_synth sugarbee_wav PkgConfig::SDL2 m)
    sugarbee_warnings(test_audio_session)
    add_test(NAME audio_session COMMAND test_audio_session)
    set_tests_properties(audio_session PROPERTIES TIMEOUT 5 ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    add_executable(test_audio_bank_sdl tests/test_audio_bank_sdl.c src/sb_audio_tone.c)
    target_include_directories(test_audio_bank_sdl PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(test_audio_bank_sdl PRIVATE sugarbee_synth sugarbee_wav PkgConfig::SDL2 m)
    sugarbee_warnings(test_audio_bank_sdl)
    add_test(NAME audio_bank_sdl COMMAND test_audio_bank_sdl)
    add_test(NAME audio_session_output COMMAND sbee-session-test)
    set_tests_properties(audio_session_output PROPERTIES TIMEOUT 15 ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    add_test(NAME audio_filter_output COMMAND sbee-filter-test)
    set_tests_properties(audio_filter_output PROPERTIES TIMEOUT 25
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    add_test(NAME audio_pitch_output COMMAND sbee-pitch-test)
    set_tests_properties(audio_pitch_output PROPERTIES TIMEOUT 30
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    add_test(NAME audio_noise_switch_output COMMAND sbee-noise-switch-test)
    set_tests_properties(audio_noise_switch_output PROPERTIES TIMEOUT 30
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    add_test(NAME audio_waveform_output COMMAND sbee-waveform-test)
    set_tests_properties(audio_waveform_output PROPERTIES TIMEOUT 20
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    add_test(NAME audio_pulse_output COMMAND sbee-pulse-test)
    set_tests_properties(audio_pulse_output PROPERTIES TIMEOUT 20
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    add_test(NAME audio_saw_output COMMAND sbee-saw-test)
    set_tests_properties(audio_saw_output PROPERTIES TIMEOUT 15
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    add_test(NAME audio_noise_output COMMAND sbee-noise-test)
    set_tests_properties(audio_noise_output PROPERTIES TIMEOUT 15
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    add_test(NAME audio_bank_output COMMAND sbee-bank-test)
    set_tests_properties(audio_bank_sdl audio_bank_output PROPERTIES TIMEOUT 10
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    add_executable(test_audio_pan tests/test_audio_pan.c)
    target_link_libraries(test_audio_pan PRIVATE sugarbee_audio)
    sugarbee_warnings(test_audio_pan)
    add_test(NAME audio_pan_samples COMMAND test_audio_pan)
    set_tests_properties(audio_pan_samples PROPERTIES TIMEOUT 5)
    add_test(NAME audio_pan_output COMMAND sbee-pan-test)
    set_tests_properties(audio_pan_output PROPERTIES TIMEOUT 10
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    add_executable(test_audio_triangle tests/test_audio_triangle.c)
    target_link_libraries(test_audio_triangle PRIVATE sugarbee_audio)
    sugarbee_warnings(test_audio_triangle)
    add_test(NAME audio_triangle_samples COMMAND test_audio_triangle)
    set_tests_properties(audio_triangle_samples PROPERTIES TIMEOUT 5)
    add_test(NAME audio_triangle_output COMMAND sbee-triangle-test)
    set_tests_properties(audio_triangle_output PROPERTIES TIMEOUT 10
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    add_executable(test_audio_tone tests/test_audio_tone.c)
    target_link_libraries(test_audio_tone PRIVATE sugarbee_audio)
    sugarbee_warnings(test_audio_tone)
    add_test(NAME audio_tone_samples COMMAND test_audio_tone)
    set_tests_properties(audio_tone_samples PROPERTIES TIMEOUT 5)
    add_test(NAME audio_tone_output COMMAND sbee-audio-test)
    set_tests_properties(audio_tone_output PROPERTIES TIMEOUT 10
        ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    add_executable(test_lua_wav tests/test_lua_wav.c)
    target_link_libraries(test_lua_wav PRIVATE ${SUGARBEE_HOST_TEST_LIBRARIES})
    sugarbee_warnings(test_lua_wav)

    add_executable(test_lua_synth tests/test_lua_synth.c)
    target_link_libraries(test_lua_synth PRIVATE ${SUGARBEE_HOST_TEST_LIBRARIES})
    sugarbee_warnings(test_lua_synth)
    add_test(NAME lua_synth COMMAND test_lua_synth)
    set_tests_properties(lua_synth PROPERTIES TIMEOUT 5 ENVIRONMENT "SDL_AUDIODRIVER=dummy")

    if(SUGARBEE_BUILD_PACKER)
        add_test(NAME wav_lab_pack COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/wav-lab"
            "-DSTAGE=${CMAKE_CURRENT_BINARY_DIR}/wav-lab/source"
            "-DGENERATOR=$<TARGET_FILE:generate_wav_lab>"
            "-DPACKER=$<TARGET_FILE:sbee-pack>"
            "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/wav-lab/wav-lab.vcart"
            -P "${PROJECT_SOURCE_DIR}/tests/wav-lab/prepare.cmake")
        set_tests_properties(wav_lab_pack PROPERTIES FIXTURES_SETUP wav_lab TIMEOUT 10)
        foreach(mode IN ITEMS output unavailable)
            add_test(NAME lua_wav_${mode} COMMAND test_lua_wav
                "${CMAKE_CURRENT_BINARY_DIR}/wav-lab/wav-lab.vcart" ${mode})
            set_tests_properties(lua_wav_${mode} PROPERTIES FIXTURES_REQUIRED wav_lab TIMEOUT 10
                ENVIRONMENT "SDL_AUDIODRIVER=dummy;SDL_VIDEODRIVER=dummy")
        endforeach()
        add_test(NAME pong_audio_pack COMMAND sbee-pack
            -i "${PROJECT_SOURCE_DIR}/examples/pong-lab"
            -o "${CMAKE_CURRENT_BINARY_DIR}/pong-audio-test.vcart" -t "Pong Lab")
        set_tests_properties(pong_audio_pack PROPERTIES FIXTURES_SETUP pong_audio TIMEOUT 5)
        foreach(mode IN ITEMS pong pong-unavailable)
            add_test(NAME pong_audio_${mode} COMMAND test_lua_synth
                "${CMAKE_CURRENT_BINARY_DIR}/pong-audio-test.vcart" ${mode})
            set_tests_properties(pong_audio_${mode} PROPERTIES FIXTURES_REQUIRED pong_audio
                TIMEOUT 5 ENVIRONMENT "SDL_AUDIODRIVER=dummy;SDL_VIDEODRIVER=dummy")
        endforeach()
        add_test(NAME musical_synth_lab_pack COMMAND sbee-pack
            -i "${PROJECT_SOURCE_DIR}/examples/musical-synth-lab"
            -o "${CMAKE_CURRENT_BINARY_DIR}/musical-synth-lab-test.vcart" -t "Musical Synth Lab"
            --uuid 7499fd87-b134-47f5-bdf8-0889dce5ad9d)
        set_tests_properties(musical_synth_lab_pack PROPERTIES FIXTURES_SETUP musical_synth_lab TIMEOUT 5)
        foreach(mode IN ITEMS music music-unavailable)
            add_test(NAME musical_synth_lab_${mode} COMMAND test_lua_synth
                "${CMAKE_CURRENT_BINARY_DIR}/musical-synth-lab-test.vcart" ${mode})
            set_tests_properties(musical_synth_lab_${mode} PROPERTIES FIXTURES_REQUIRED musical_synth_lab
                TIMEOUT 5 ENVIRONMENT "SDL_AUDIODRIVER=dummy;SDL_VIDEODRIVER=dummy")
        endforeach()
        add_test(NAME synth_lab_pack COMMAND sbee-pack
            -i "${PROJECT_SOURCE_DIR}/examples/synth-lab"
            -o "${CMAKE_CURRENT_BINARY_DIR}/synth-lab-test.vcart" -t "Synth Lab"
            --uuid 5f5040f9-6d44-485d-a923-f977d76cd2de)
        set_tests_properties(synth_lab_pack PROPERTIES FIXTURES_SETUP synth_lab TIMEOUT 5)
        foreach(mode IN ITEMS audible unavailable)
            add_test(NAME synth_lab_${mode} COMMAND test_lua_synth
                "${CMAKE_CURRENT_BINARY_DIR}/synth-lab-test.vcart" $<$<STREQUAL:${mode},unavailable>:unavailable>)
            set_tests_properties(synth_lab_${mode} PROPERTIES FIXTURES_REQUIRED synth_lab
                TIMEOUT 5 ENVIRONMENT "SDL_AUDIODRIVER=dummy;SDL_VIDEODRIVER=dummy")
        endforeach()
    endif()

    add_executable(test_audio_host tests/test_audio_host.c)
    target_link_libraries(test_audio_host PRIVATE ${SUGARBEE_HOST_TEST_LIBRARIES})
    target_link_options(test_audio_host PRIVATE
        "-Wl,--wrap=SDL_OpenAudioDevice" "-Wl,--wrap=SDL_CloseAudioDevice")
    sugarbee_warnings(test_audio_host)
    foreach(mode IN ITEMS silence unavailable open_failure)
        add_test(NAME audio_host_${mode} COMMAND test_audio_host ${mode})
        set_tests_properties(audio_host_${mode} PROPERTIES TIMEOUT 10
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
    endforeach()

    add_executable(test_save_selection tests/test_save_selection.c)
    target_link_libraries(test_save_selection PRIVATE ${SUGARBEE_HOST_TEST_LIBRARIES})
    sugarbee_warnings(test_save_selection)
    add_test(NAME host_save_selection COMMAND test_save_selection)
    set_tests_properties(host_save_selection PROPERTIES TIMEOUT 5)

    add_executable(test_host_save tests/test_host_save.c)
    target_link_libraries(test_host_save PRIVATE ${SUGARBEE_HOST_TEST_LIBRARIES})
    target_link_options(test_host_save PRIVATE
        "-Wl,--wrap=fsync" "-Wl,--wrap=renameat" "-Wl,--wrap=write" "-Wl,--wrap=mkdirat")
    sugarbee_warnings(test_host_save)
    add_test(NAME host_save_storage COMMAND test_host_save)
    set_tests_properties(host_save_storage PROPERTIES TIMEOUT 15)



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

    add_executable(test_save_interruption tests/test_save_interruption.c)
    target_link_libraries(test_save_interruption PRIVATE sugarbee_save)
    target_link_options(test_save_interruption PRIVATE
        "-Wl,--wrap=write" "-Wl,--wrap=fsync" "-Wl,--wrap=renameat")
    sugarbee_warnings(test_save_interruption)
    foreach(point IN ITEMS partial_write file_synced replaced)
        add_test(NAME save_interruption_${point} COMMAND test_save_interruption ${point})
        set_tests_properties(save_interruption_${point} PROPERTIES TIMEOUT 15)
    endforeach()

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

    add_executable(test_save_location tests/test_save_location.c)
    target_link_libraries(test_save_location PRIVATE sugarbee_save)
    target_link_options(test_save_location PRIVATE "-Wl,--wrap=mkdirat" "-Wl,--wrap=fsync")
    sugarbee_warnings(test_save_location)
    add_test(NAME save_location COMMAND test_save_location)
    set_tests_properties(save_location PROPERTIES TIMEOUT 15)



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
        add_test(NAME pong_audio_behavior COMMAND "${LUA54_INTERPRETER}"
            "${PROJECT_SOURCE_DIR}/tests/pong-audio/test.lua"
            "${PROJECT_SOURCE_DIR}/examples/pong-lab/main.lua")
        set_tests_properties(pong_audio_behavior PROPERTIES TIMEOUT 5)
        add_test(NAME musical_synth_lab_behavior
            COMMAND "${LUA54_INTERPRETER}" "${PROJECT_SOURCE_DIR}/tests/musical-synth-lab/test.lua"
                "${PROJECT_SOURCE_DIR}/examples/musical-synth-lab/main.lua")
        set_tests_properties(musical_synth_lab_behavior PROPERTIES TIMEOUT 5)
        add_test(NAME save_lab_behavior
            COMMAND "${LUA54_INTERPRETER}"
                "${PROJECT_SOURCE_DIR}/tests/save-lab/test.lua"
                "${PROJECT_SOURCE_DIR}/examples/save-lab/main.lua")
        set_tests_properties(save_lab_behavior PROPERTIES TIMEOUT 5)
        foreach(cartridge IN ITEMS graphics-demo moving-box sprite-demo
                asset-demo sprite-sheet-demo pong-lab invaders-lab pacman-demo)
            add_test(NAME cartridge_smoke_${cartridge}
                COMMAND "${LUA54_INTERPRETER}"
                    "${PROJECT_SOURCE_DIR}/tests/cartridge_smoke.lua"
                    "${PROJECT_SOURCE_DIR}/examples/${cartridge}" 3000)
        endforeach()
    endif()
endif()
