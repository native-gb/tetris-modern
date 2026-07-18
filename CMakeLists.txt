cmake_minimum_required(VERSION 3.21)

project(native_gb_tetris_modern VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TETRIS_STRICT "Enable strict compiler warnings" ON)
option(TETRIS_WARN_AS_ERROR "Treat warnings as errors" ON)
option(TETRIS_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(TETRIS_BUILD_APP "Build the SDL3 desktop application" ON)
set(TETRIS_TEST_ROM "" CACHE FILEPATH "Optional supported ROM used by extraction tests")
set(TETRIS_DEFAULT_ROM "${CMAKE_CURRENT_SOURCE_DIR}/roms/Tetris (JUE) (V1.1) [!].gb")
if(NOT TETRIS_TEST_ROM AND EXISTS "${TETRIS_DEFAULT_ROM}")
    set(TETRIS_TEST_ROM "${TETRIS_DEFAULT_ROM}")
endif()

function(tetris_target_defaults target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(TETRIS_WARN_AS_ERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(TETRIS_STRICT)
            target_compile_options(${target} PRIVATE -Wconversion -Wsign-conversion -Wshadow)
        endif()
        if(TETRIS_WARN_AS_ERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(TETRIS_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_library(tetris-game STATIC
    src/game/board.cpp
    src/game/demo.cpp
    src/game/flow.cpp
    src/game/flow_endings.cpp
    src/game/high_scores.cpp
    src/game/multiplayer.cpp
    src/game/piece.cpp
    src/game/randomizer.cpp
    src/game/replay.cpp
    src/game/rules.cpp
    src/game/single_player.cpp
)
target_include_directories(tetris-game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
tetris_target_defaults(tetris-game)

add_library(tetris-content STATIC
    src/content/audio.cpp
    src/content/catalog.cpp
    src/content/rom.cpp
    src/content/sha1.cpp
)
target_include_directories(tetris-content PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(tetris-content PUBLIC tetris-game)
tetris_target_defaults(tetris-content)

add_library(tetris-presentation STATIC
    src/presentation/effects.cpp
    src/presentation/settings.cpp
)
target_include_directories(tetris-presentation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(tetris-presentation PUBLIC tetris-game tetris-content)
tetris_target_defaults(tetris-presentation)

if(TETRIS_BUILD_APP)
    set(TETRIS_GUBSY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Splonks/gubsy" CACHE PATH "Gubsy source tree")
    set(TETRIS_GB_AUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../gb-audio" CACHE PATH "gb-audio source tree")

    set(GUB_MODE consumer CACHE STRING "" FORCE)
    set(GUB_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
    set(GUB_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(GUB_ENABLE_LUA_MOD_HOST OFF CACHE BOOL "" FORCE)
    set(GUB_ENABLE_STEAM OFF CACHE BOOL "" FORCE)
    set(GUB_FETCH_DEPS ON CACHE BOOL "" FORCE)
    set(GUB_SDL_DEPS fetch CACHE STRING "" FORCE)
    if(EXISTS "${TETRIS_GUBSY_DIR}/CMakeLists.txt")
        add_subdirectory("${TETRIS_GUBSY_DIR}" gubsy)
    else()
        FetchContent_Declare(gubsy
            GIT_REPOSITORY https://github.com/wegfawefgawefg/gubsy.git
            GIT_TAG f66db5113d05a62de5a854106f831796301128c7)
        FetchContent_MakeAvailable(gubsy)
        set(TETRIS_GUBSY_DIR "${gubsy_SOURCE_DIR}")
    endif()
    if(EXISTS "${TETRIS_GB_AUDIO_DIR}/CMakeLists.txt")
        add_subdirectory("${TETRIS_GB_AUDIO_DIR}" gb-audio)
    else()
        FetchContent_Declare(gb_audio
            GIT_REPOSITORY https://github.com/native-gb/gb-audio.git
            GIT_TAG 13778d0e1e65df53eb6ede780948e86c1c4a84df)
        FetchContent_MakeAvailable(gb_audio)
        set(TETRIS_GB_AUDIO_DIR "${gb_audio_SOURCE_DIR}")
    endif()

    add_library(tetris-rendering STATIC
        src/presentation/endings.cpp
        src/presentation/renderer.cpp
        src/presentation/ui.cpp
    )
    target_include_directories(tetris-rendering PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${TETRIS_GUBSY_DIR}/include")
    target_link_libraries(tetris-rendering PUBLIC
        tetris-game tetris-content tetris-presentation SDL3::SDL3)
    tetris_target_defaults(tetris-rendering)

    add_library(tetris-audio STATIC
        src/audio/director.cpp
        src/audio/music.cpp
        src/audio/output.cpp
        src/audio/sound_effects.cpp
    )
    target_include_directories(tetris-audio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(tetris-audio PUBLIC tetris-game tetris-content native-gb::audio SDL3::SDL3)
    tetris_target_defaults(tetris-audio)

    add_library(tetris-app-input STATIC src/app/input.cpp)
    target_include_directories(tetris-app-input PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(tetris-app-input PUBLIC tetris-game gubsy::engine)
    tetris_target_defaults(tetris-app-input)

    add_executable(native-gb-tetris-modern
        src/app/app.cpp
        src/app/controls_menu.cpp
        src/app/debug_ui.cpp
        src/app/main.cpp
        src/app/persistence.cpp
    )
    target_include_directories(native-gb-tetris-modern PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${TETRIS_GUBSY_DIR}"
        "${TETRIS_GUBSY_DIR}/imgui"
        "${TETRIS_GUBSY_DIR}/imgui/backends")
    target_compile_definitions(native-gb-tetris-modern PRIVATE
        TETRIS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        TETRIS_GUBSY_ASSETS_DIR="${TETRIS_GUBSY_DIR}/src/assets")
    target_link_libraries(native-gb-tetris-modern PRIVATE
        tetris-game tetris-content tetris-presentation tetris-rendering
        tetris-audio tetris-app-input gubsy::engine)
    tetris_target_defaults(native-gb-tetris-modern)
endif()

if(BUILD_TESTING)
    add_executable(tetris-game-tests tests/game_tests.cpp)
    target_link_libraries(tetris-game-tests PRIVATE tetris-game)
    tetris_target_defaults(tetris-game-tests)
    add_test(NAME tetris_game COMMAND tetris-game-tests)

    add_executable(tetris-simulation-tests tests/simulation_tests.cpp)
    target_link_libraries(tetris-simulation-tests PRIVATE tetris-game)
    tetris_target_defaults(tetris-simulation-tests)
    add_test(NAME tetris_simulation COMMAND tetris-simulation-tests)

    add_executable(tetris-multiplayer-tests tests/multiplayer_tests.cpp)
    target_link_libraries(tetris-multiplayer-tests PRIVATE tetris-game)
    tetris_target_defaults(tetris-multiplayer-tests)
    add_test(NAME tetris_multiplayer COMMAND tetris-multiplayer-tests)

    add_executable(tetris-flow-tests tests/flow_tests.cpp)
    target_link_libraries(tetris-flow-tests PRIVATE tetris-game)
    tetris_target_defaults(tetris-flow-tests)
    add_test(NAME tetris_flow COMMAND tetris-flow-tests)

    add_executable(tetris-content-tests tests/content_tests.cpp)
    target_link_libraries(tetris-content-tests PRIVATE tetris-content)
    if(TETRIS_TEST_ROM)
        target_compile_definitions(tetris-content-tests PRIVATE TETRIS_TEST_ROM="${TETRIS_TEST_ROM}")
    endif()
    tetris_target_defaults(tetris-content-tests)
    add_test(NAME tetris_content COMMAND tetris-content-tests)

    add_executable(tetris-persistence-tests
        tests/persistence_tests.cpp
        src/app/persistence.cpp)
    target_include_directories(tetris-persistence-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(tetris-persistence-tests PRIVATE tetris-game tetris-presentation)
    tetris_target_defaults(tetris-persistence-tests)
    add_test(NAME tetris_persistence COMMAND tetris-persistence-tests)

    if(TETRIS_BUILD_APP)
        add_executable(tetris-audio-tests tests/audio_tests.cpp)
        target_link_libraries(tetris-audio-tests PRIVATE tetris-audio)
        if(TETRIS_TEST_ROM)
            target_compile_definitions(tetris-audio-tests PRIVATE TETRIS_TEST_ROM="${TETRIS_TEST_ROM}")
        endif()
        tetris_target_defaults(tetris-audio-tests)
        add_test(NAME tetris_audio COMMAND tetris-audio-tests)
        set_tests_properties(tetris_audio PROPERTIES
            ENVIRONMENT "SDL_AUDIODRIVER=dummy")

        add_executable(tetris-audio-output-tests tests/audio_output_tests.cpp)
        target_link_libraries(tetris-audio-output-tests PRIVATE tetris-audio)
        if(TETRIS_TEST_ROM)
            target_compile_definitions(tetris-audio-output-tests PRIVATE
                TETRIS_TEST_ROM="${TETRIS_TEST_ROM}")
        endif()
        tetris_target_defaults(tetris-audio-output-tests)
        add_test(NAME tetris_audio_output COMMAND tetris-audio-output-tests)
        set_tests_properties(tetris_audio_output PROPERTIES
            ENVIRONMENT "SDL_AUDIODRIVER=dummy")

        add_executable(tetris-music-trace tools/music_trace.cpp)
        target_link_libraries(tetris-music-trace PRIVATE tetris-audio)
        tetris_target_defaults(tetris-music-trace)

        add_executable(tetris-presentation-tests tests/presentation_tests.cpp)
        target_link_libraries(tetris-presentation-tests PRIVATE tetris-rendering)
        if(TETRIS_TEST_ROM)
            target_compile_definitions(tetris-presentation-tests PRIVATE
                TETRIS_TEST_ROM="${TETRIS_TEST_ROM}")
        endif()
        tetris_target_defaults(tetris-presentation-tests)
        add_test(NAME tetris_presentation COMMAND tetris-presentation-tests)
        set_tests_properties(tetris_presentation PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

        add_executable(tetris-input-tests tests/input_tests.cpp)
        target_link_libraries(tetris-input-tests PRIVATE tetris-app-input)
        tetris_target_defaults(tetris-input-tests)
        add_test(NAME tetris_input COMMAND tetris-input-tests)
        set_tests_properties(tetris_input PROPERTIES
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
    endif()
endif()
