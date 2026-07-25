cmake_minimum_required(VERSION 3.16)
project(electraone_cli CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# ---- RtMidi ---------------------------------------------------------------
find_package(RtMidi CONFIG QUIET)
if(NOT RtMidi_FOUND)
  message(STATUS "RtMidi not found via find_package, fetching from source")
  set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  # Static, not RtMidi's own default (shared): keeps electraone/electraone_api
  # self-contained everywhere, instead of needing librtmidi.so/.dylib/.dll
  # discoverable alongside the binary at runtime (a real gotcha on Windows,
  # which has no rpath-equivalent by default).
  set(RTMIDI_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    rtmidi
    GIT_REPOSITORY https://github.com/thestk/rtmidi.git
    GIT_TAG 6.0.0
  )
  FetchContent_MakeAvailable(rtmidi)
  # RtMidi.cpp itself trips Clang's VLA-extension warning; it's third-party
  # source we don't control, so silence it on that target specifically.
  if(TARGET rtmidi AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(rtmidi PRIVATE -Wno-vla-cxx-extension)
  endif()
endif()

# ---- CLI11 -----------------------------------------------------------------
find_package(CLI11 CONFIG QUIET)
if(NOT CLI11_FOUND)
  message(STATUS "CLI11 not found via find_package, fetching from source")
  FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.6.2
  )
  FetchContent_MakeAvailable(cli11)
endif()

# ---- ArduinoJson ------------------------------------------------------------
FetchContent_Declare(
  ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.2.1
)
FetchContent_MakeAvailable(ArduinoJson)

# Resolve once: find_package gives a namespaced target, FetchContent gives a bare one.
set(RTMIDI_TARGET $<IF:$<TARGET_EXISTS:RtMidi::rtmidi>,RtMidi::rtmidi,rtmidi>)
set(CLI11_TARGET $<IF:$<TARGET_EXISTS:CLI11::CLI11>,CLI11::CLI11,CLI11>)

# ---- electraone API library ------------------------------------------------
# C++ client library for the Electra One SysEx protocol - what the CLI below
# is built on top of, packaged for 3rd-party apps to link against directly.
# Public header only depends on the STL; RtMidi and ArduinoJson are
# implementation details of the .cpp, but RtMidi must still be linked
# PUBLIC so consumers of this library get it (and its system MIDI backend)
# transitively without needing to know about it.
add_library(electraone_api STATIC
  src/api/ElectraOneClient.cpp
  src/sysex.cpp
  src/midi_transport.cpp
)
set_target_properties(electraone_api PROPERTIES OUTPUT_NAME electraone)
if(WIN32)
  # On Windows, every linked executable gets a companion import library
  # (TARGET_IMPLIB) even though it exports nothing - and CMake names it
  # after the target, so the "electraone" CLI executable's throwaway
  # import lib would otherwise land right next to this library's real
  # electraone.lib and silently overwrite it (whichever links last wins),
  # leaving anything that links electraone_api afterwards with unresolved
  # symbols. Route this target's archive output to its own directory so
  # the two "electraone.lib" files can never collide. Not needed on
  # macOS/Linux, where executables don't get a companion import library,
  # and examples/Makefile hardcodes the default build/libelectraone.a path.
  set_target_properties(electraone_api PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
endif()
target_include_directories(electraone_api
  PUBLIC include
  PRIVATE src
)
target_link_libraries(electraone_api
  PUBLIC ${RTMIDI_TARGET}
  PRIVATE ArduinoJson
)

install(TARGETS electraone_api ARCHIVE DESTINATION lib)
install(FILES
  include/electraone/ElectraOneClient.hpp
  src/sysex.hpp
  DESTINATION include/electraone
)

# ---- electraone CLI ------------------------------------------------------------
add_executable(electraone
  src/main.cpp
  src/sysex.cpp
  src/midi_transport.cpp
  src/commands/runner.cpp
  src/commands/human_format.cpp
  src/commands/terminal.cpp
  src/commands/progress.cpp
  src/commands/event_decoder.cpp
  src/commands/info_commands.cpp
  src/commands/preset_commands.cpp
  src/commands/lua_commands.cpp
  src/commands/overrides_commands.cpp
  src/commands/persisted_commands.cpp
  src/commands/performance_commands.cpp
  src/commands/config_commands.cpp
  src/commands/snapshot_commands.cpp
  src/commands/capture_commands.cpp
  src/commands/control_commands.cpp
  src/commands/ui_commands.cpp
  src/commands/events_commands.cpp
  src/commands/logger_commands.cpp
  src/commands/window_commands.cpp
  src/commands/files_commands.cpp
  src/commands/raw_commands.cpp
)

target_include_directories(electraone PRIVATE src include)

target_link_libraries(electraone PRIVATE
  ${RTMIDI_TARGET}
  ${CLI11_TARGET}
  ArduinoJson
)

install(TARGETS electraone RUNTIME DESTINATION bin)

# ---- examples ---------------------------------------------------------------
# Built the same way on every platform via `cmake --build build` - no manual
# per-OS link line needed, since electraone_api already carries its RtMidi
# (and system MIDI backend) dependency as a PUBLIC link requirement.
option(ELECTRAONE_BUILD_EXAMPLES "Build the programs under examples/" ON)
if(ELECTRAONE_BUILD_EXAMPLES)
  add_executable(basic_usage examples/basic_usage.cpp)
  target_link_libraries(basic_usage PRIVATE electraone_api)
endif()

# ---- unit tests ---------------------------------------------------------------
# Covers the electraone_api library's hardware-independent logic: the SysEx
# envelope encode/decode (sysex.cpp), the MD5 implementation the File
# Transfer API's Commit step needs, and the event-description formatting -
# not Client's command methods themselves, since those need a live MIDI
# connection (RtMidi) rather than being pure logic. Only built when this is
# the top-level CMake project, so embedding electraone_api via
# add_subdirectory in another project doesn't pull in our test target too.
option(ELECTRAONE_BUILD_TESTS "Build unit tests" ON)
if(ELECTRAONE_BUILD_TESTS AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.3
  )
  FetchContent_MakeAvailable(doctest)

  enable_testing()

  add_executable(electraone_tests
    tests/test_main.cpp
    tests/test_sysex.cpp
    tests/test_md5.cpp
    tests/test_describe_event.cpp
    tests/test_progress.cpp
    src/commands/progress.cpp
  )
  target_include_directories(electraone_tests PRIVATE src)
  target_link_libraries(electraone_tests PRIVATE electraone_api doctest::doctest)

  include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
  doctest_discover_tests(electraone_tests)
endif()
