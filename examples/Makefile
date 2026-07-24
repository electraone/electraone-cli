# Convenience Makefile for basic_usage.cpp on macOS/Linux, showing the raw
# compile/link command in one place. This isn't how the example is normally
# built - `cmake --build build` from the repo root builds it automatically
# on every platform, including Windows (see README.md). Use this Makefile if
# you want a quick single-file rebuild without re-running CMake, or just
# want to see the exact link line.
#
# Prerequisite: build the library first, from the repo root:
#   cmake -B build && cmake --build build
#
# Then:
#   cd examples && make
#   ./basic_usage

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall
BUILD_DIR := ../build
INCLUDE   := -I../include

TARGET         := basic_usage
ELECTRAONE_LIB := $(BUILD_DIR)/libelectraone.a

UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(UNAME_S),Darwin)
  RTMIDI_FETCHED_LIB := $(BUILD_DIR)/_deps/rtmidi-build/librtmidi.a
  SYSTEM_LIBS        := -framework CoreMIDI -framework CoreAudio -framework CoreFoundation
else ifeq ($(UNAME_S),Linux)
  RTMIDI_FETCHED_LIB := $(BUILD_DIR)/_deps/rtmidi-build/librtmidi.a
  SYSTEM_LIBS        := -lasound -lpthread
else
  $(error This Makefile covers macOS/Linux only (got uname '$(UNAME_S)'). On Windows, or \
under MSYS2/MinGW where 'make' works but this link line doesn't, build the example the \
cross-platform way instead: `cmake --build build` from the repo root - see README.md)
endif

# CMake either fetched RtMidi from source and built it static (predictable
# path above, per CMakeLists.txt's RTMIDI_BUILD_STATIC_LIBS), or found it
# already installed on the system (find_package) - fall back to a plain
# -lrtmidi against the system copy in the latter case.
ifneq ($(wildcard $(RTMIDI_FETCHED_LIB)),)
  RTMIDI_LIB := $(RTMIDI_FETCHED_LIB)
else
  RTMIDI_LIB := -lrtmidi
endif

.PHONY: all clean
all: $(TARGET)

$(TARGET): basic_usage.cpp $(ELECTRAONE_LIB)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $< $(ELECTRAONE_LIB) $(RTMIDI_LIB) $(SYSTEM_LIBS) -o $@

$(ELECTRAONE_LIB):
	@echo "$(ELECTRAONE_LIB) not found - build the library first:"
	@echo "  cmake -B $(BUILD_DIR) && cmake --build $(BUILD_DIR)"
	@exit 1

clean:
	rm -f $(TARGET)
