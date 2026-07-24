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
class MidiTransport {
public:
    MidiTransport();
    ~MidiTransport();

    MidiTransport(const MidiTransport&) = delete;
    MidiTransport& operator=(const MidiTransport&) = delete;

    // Prints all available MIDI output and input ports with their indices.
    static void listPorts();

    // Same enumeration as listPorts(), returned as name lists instead of printed.
    static std::vector<std::string> listOutputPortNames();
    static std::vector<std::string> listInputPortNames();

    // Opens the output+input port pair whose name contains nameSubstring
    // (case-insensitive), or, if portIndex is set, opens that index directly
    // on both the output and input port lists. Throws std::runtime_error with
    // the candidate list if the match isn't exactly one port.
    void open(const std::string& nameSubstring, std::optional<unsigned int> portIndex);

    void send(const std::vector<uint8_t>& bytes);

    // Polls for the next complete Electra SysEx message (F0 00 21 45 ...)
    // within timeoutMs. Returns std::nullopt on timeout.
    std::optional<std::vector<uint8_t>> waitForReply(int timeoutMs);

private:
    std::unique_ptr<RtMidiIn> in_;
    std::unique_ptr<RtMidiOut> out_;
};
