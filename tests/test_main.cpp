// Generates doctest's main() for the electraone_tests binary. Every other
// tests/*.cpp file just includes <doctest/doctest.h> and defines TEST_CASEs.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
