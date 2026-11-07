cmake_minimum_required(VERSION 3.16)
project(electraone_cli VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MINGW)
  # Bundle libstdc++, libgcc and winpthread so the .exe runs without MinGW DLLs.
  add_link_options(-static)
endif()

include(FetchContent)

# ---- RtMidi ---------------------------------------------------------------
find_package(RtMidi CONFIG QUIET)
if(NOT RtMidi_FOUND)
  # Debian/Ubuntu librtmidi-dev ships only a pkg-config file, no CMake config.
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(RTMIDI_PC IMPORTED_TARGET rtmidi)
  endif()
endif()
if(NOT RtMidi_FOUND AND NOT RTMIDI_PC_FOUND)
  message(STATUS "RtMidi not found via find_package or pkg-config, fetching from source")
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

# Resolve once: find_package gives a namespaced target, pkg-config an imported one, FetchContent a bare one.
if(RtMidi_FOUND)
  set(RTMIDI_TARGET RtMidi::rtmidi)
elseif(RTMIDI_PC_FOUND)
  set(RTMIDI_TARGET PkgConfig::RTMIDI_PC)
else()
  set(RTMIDI_TARGET rtmidi)
endif()
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

# ---- electraone CLI ------------------------------------------------------------
# Single source of truth for the CLI's displayed/binary name, propagated into
# the app via the generated electraone_version.hpp header below.
set(ELECTRAONE_CLI_NAME "electraone")

configure_file(
  src/electraone_version.hpp.in
  ${CMAKE_BINARY_DIR}/generated/electraone_version.hpp
  @ONLY
)

add_executable(${ELECTRAONE_CLI_NAME}
  src/main.cpp
  src/sysex.cpp
  src/midi_transport.cpp
  src/commands/runner.cpp
  src/commands/human_format.cpp
  src/commands/terminal.cpp
  src/commands/progress.cpp
  src/commands/event_decoder.cpp
  src/commands/info_commands.cpp
  src/commands/debug_commands.cpp
  src/commands/midilearn_commands.cpp
  src/commands/system_commands.cpp
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

target_include_directories(${ELECTRAONE_CLI_NAME} PRIVATE src include ${CMAKE_BINARY_DIR}/generated)

target_link_libraries(${ELECTRAONE_CLI_NAME} PRIVATE
  ${RTMIDI_TARGET}
  ${CLI11_TARGET}
  ArduinoJson
)

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
# doctest_discover_tests runs the test binary at build time, which needs an emulator when cross-compiling.
if(ELECTRAONE_BUILD_TESTS AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR
   AND (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR))
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
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC 14 false positive on vector insert + push_back in test_sysex.cpp.
    target_compile_options(electraone_tests PRIVATE -Wno-stringop-overflow)
  endif()

  include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
  doctest_discover_tests(electraone_tests)
endif()

# ==========================================
# 1. INSTALLATION DIRECTIVES 
# ==========================================
include(GNUInstallDirs)

install(TARGETS ${ELECTRAONE_CLI_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT electraone)
install(TARGETS electraone_api ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT electraone)
if(RTMIDI_TARGET STREQUAL "rtmidi")
  # No system RtMidi to depend on (e.g. Windows): ship the fetched static lib for libelectraone.a users.
  install(FILES $<TARGET_LINKER_FILE:rtmidi> DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT electraone)
endif()
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/electraone/ElectraOneClient.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/electraone/sysex.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/electraone
    COMPONENT electraone
)

# ==========================================
# CPACK CONFIGURATION
# ==========================================
set(CPACK_PACKAGE_NAME "electraone")
set(CPACK_PACKAGE_VENDOR "Electra One")
set(CPACK_PACKAGE_VERSION "1.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A cross-platform command-line suite and C++ client library for the Electra One SysEx protocol.")
set(CPACK_PACKAGE_CONTACT "info@electra.one")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# Package only our component; FetchContent deps (RtMidi, ArduinoJson, doctest) add their own install rules.
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${PROJECT_NAME};electraone;/")

if(WIN32)
  # ZIP always; NSIS installer when makensis is available (apt install nsis).
  set(CPACK_GENERATOR "ZIP")
  find_program(MAKENSIS_EXECUTABLE makensis)
  if(MAKENSIS_EXECUTABLE)
    list(APPEND CPACK_GENERATOR "NSIS")
  endif()
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "electraone")
  set(CPACK_NSIS_DISPLAY_NAME "electraone")
  set(CPACK_NSIS_MODIFY_PATH ON)
  set(CPACK_NSIS_URL_INFO_ABOUT "https://electra.one")
else()
  set(CPACK_GENERATOR "RPM;DEB;TGZ")
endif()

# Debian Target Configuration
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://electra.one")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# libelectraone.a consumers need the RtMidi headers and library too.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "librtmidi-dev")

# RPM Target Configuration (Fedora/RHEL/openSUSE)
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_PACKAGE_REQUIRES "libjack.so.0()(64bit), libasound.so.2()(64bit), rtmidi-devel")
set(CPACK_RPM_PACKAGE_URL "https://electra.one")

include(CPack)

