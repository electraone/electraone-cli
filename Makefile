# Convenience wrapper around clang-format for consistent source formatting.
# This project's actual build system is CMake (see CMakeLists.txt) - this
# Makefile exists only for `make format`/`make format-check`, using the
# style defined in .clang-format.

CLANG_FORMAT ?= clang-format

SOURCES := $(shell find src include tests examples -name '*.cpp' -o -name '*.hpp')

.PHONY: format format-check
format:
	$(CLANG_FORMAT) -i $(SOURCES)

# Fails (nonzero exit) if any file isn't already formatted, without modifying
# anything - for CI or a pre-commit check.
format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(SOURCES)
