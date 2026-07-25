#pragma once

#include <string>

#include "electraone/sysex.hpp"

namespace commands
{

    // Renders an incoming unsolicited Electra SysEx message (ACK/NACK, Preset
    // Switch, Pot Touch, Log Message, MIDI Learn Info, ...) as a single
    // human-readable line, for use in `events listen` / `logger listen`.
    std::string describeEvent(const sysex::ParsedResponse &resp);

    // Prints describeEvent(resp) to stdout, prefixed with the current wall-clock
    // time ([HH:MM:SS.mmm]). Used as the listen callback for both `events
    // listen` and `logger listen`, so a merged log+event stream stays easy to
    // read chronologically - every line gets a timestamp, not just log lines,
    // since once the two are interleaved a consistent host-side clock is what
    // makes the stream readable at a glance.
    void printEventWithTimestamp(const sysex::ParsedResponse &resp);

} // namespace commands
