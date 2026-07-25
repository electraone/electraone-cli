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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class RtMidiIn;
class RtMidiOut;

// Thin wrapper around RtMidiIn/RtMidiOut for talking to the Electra One CTRL
// port: port discovery, sending a SysEx message, and polling for a reply.
// Assumes one request is in flight at a time, matching how the CLI issues a
// single command per invocation.
class MidiTransport
{
public:
    MidiTransport();
    ~MidiTransport();

    MidiTransport(const MidiTransport &) = delete;
    MidiTransport &operator=(const MidiTransport &) = delete;

    // Prints all available MIDI output and input ports with their indices.
    static void listPorts();

    // Same enumeration as listPorts(), returned as name lists instead of printed.
    static std::vector<std::string> listOutputPortNames();
    static std::vector<std::string> listInputPortNames();

    // Opens the output+input port pair whose name contains nameSubstring
    // (case-insensitive). outPortIndex/inPortIndex, if set, each open that
    // index directly on the corresponding port list instead of name-matching
    // - independently, since the output and input port lists aren't always
    // the same length (e.g. on Windows, a software-only synth can appear as
    // an extra output port with no matching input, shifting every real
    // device's output index out of alignment with its input index). Throws
    // std::runtime_error with the candidate list if a name match isn't
    // exactly one port, or if an explicit index is out of range.
    void open(const std::string &nameSubstring,
              std::optional<unsigned int> outPortIndex,
              std::optional<unsigned int> inPortIndex);

    void send(const std::vector<uint8_t> &bytes);

    // Polls for the next complete Electra SysEx message (F0 00 21 45 ...)
    // within timeoutMs. Returns std::nullopt on timeout.
    std::optional<std::vector<uint8_t>> waitForReply(int timeoutMs);

private:
    std::unique_ptr<RtMidiIn> in_;
    std::unique_ptr<RtMidiOut> out_;
};
