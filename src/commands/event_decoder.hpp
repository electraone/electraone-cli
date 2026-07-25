/*
* Electra One MIDI Controller host tools
* See COPYRIGHT file at the top of the source tree.
*
* This product includes software developed by the
* Electra One Project (http://electra.one/).
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.
*/

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
