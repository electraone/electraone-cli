#pragma once

#include <string>

#include "electraone/sysex.hpp"

namespace commands {

// Renders an incoming unsolicited Electra SysEx message (ACK/NACK, Preset
// Switch, Pot Touch, Log Message, MIDI Learn Info, ...) as a single
// human-readable line, for use in `events listen` / `logger listen`.
std::string describeEvent(const sysex::ParsedResponse& resp);

}  // namespace commands
