# Cross-compile for 64-bit Windows with MinGW-w64 (Debian/Ubuntu: apt install mingw-w64).
#   cmake -S . -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_TRIPLET x86_64-w64-mingw32)

# The -posix variants provide std::thread (the -win32 ones do not).
find_program(CMAKE_C_COMPILER NAMES ${MINGW_TRIPLET}-gcc-posix ${MINGW_TRIPLET}-gcc REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES ${MINGW_TRIPLET}-g++-posix ${MINGW_TRIPLET}-g++ REQUIRED)
find_program(CMAKE_RC_COMPILER NAMES ${MINGW_TRIPLET}-windres REQUIRED)

set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_TRIPLET})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Keep pkg-config from returning the host's Linux packages (e.g. rtmidi.pc).
set(ENV{PKG_CONFIG_LIBDIR} /usr/${MINGW_TRIPLET}/lib/pkgconfig)
set(ENV{PKG_CONFIG_PATH} "")

# Run cross-built test binaries through wine when it is installed.
find_program(WINE_EXECUTABLE wine)
if(WINE_EXECUTABLE)
  set(CMAKE_CROSSCOMPILING_EMULATOR ${WINE_EXECUTABLE})
endif()
