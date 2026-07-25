#include <doctest/doctest.h>

#include <commands/progress.hpp>

using commands::formatProgressBar;

TEST_CASE(
    "formatProgressBar: 0% shows an empty bar with the arrow at the start")
{
    auto line = formatProgressBar(0, 8054, 10);
    CHECK(line == "[>         ]   0%    0/8054 bytes");
}

TEST_CASE("formatProgressBar: 100% shows a fully filled bar with no arrow")
{
    auto line = formatProgressBar(8054, 8054, 10);
    CHECK(line == "[==========] 100% 8054/8054 bytes");
}

TEST_CASE("formatProgressBar: mid-progress rounds to the nearest bar segment")
{
    auto line = formatProgressBar(5, 10, 10);
    CHECK(line == "[=====>    ]  50%  5/10 bytes");
}

TEST_CASE(
    "formatProgressBar: total == 0 renders as complete instead of dividing by zero")
{
    auto line = formatProgressBar(0, 0, 10);
    CHECK(line == "[==========] 100% 0/0 bytes");
}

TEST_CASE("formatProgressBar: sent is clamped to total")
{
    auto overshoot = formatProgressBar(999, 100, 10);
    auto exact = formatProgressBar(100, 100, 10);
    CHECK(overshoot == exact);
}

TEST_CASE(
    "formatProgressBar: line length stays constant across updates for a given total")
{
    // This is the whole point of padding the numeric fields to total's width
    // - the caller redraws the line in place with '\r', so it must never
    // shrink (which would leave stale characters from a longer previous line).
    size_t total = 8054;
    size_t len0 = formatProgressBar(0, total).size();
    for (size_t sent :
         { static_cast<size_t>(1), total / 4, total / 2, total - 1, total }) {
        CHECK(formatProgressBar(sent, total).size() == len0);
    }
}
