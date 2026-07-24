#include "midi_transport.hpp"

#include <RtMidi.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "electraone/sysex.hpp"

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Finds the single port index whose name contains needle (case-insensitive).
// Throws std::runtime_error listing all candidates if the match count != 1.
unsigned int findPortByName(const std::vector<std::string>& names,
                             const std::string& needle, const char* direction) {
    std::string needleLower = toLower(needle);
    std::vector<unsigned int> matches;
    for (unsigned int i = 0; i < names.size(); ++i) {
        if (toLower(names[i]).find(needleLower) != std::string::npos) {
            matches.push_back(i);
        }
    }
    if (matches.size() == 1) {
        return matches.front();
    }
    std::ostringstream os;
    os << (matches.empty() ? "no " : "multiple ") << direction
       << " ports match \"" << needle << "\". Available " << direction
       << " ports:\n";
    for (unsigned int i = 0; i < names.size(); ++i) {
        os << "  [" << i << "] " << names[i] << "\n";
    }
    os << "Use --port with a more specific substring, or --port-index to pick one explicitly.";
    throw std::runtime_error(os.str());
}

}  // namespace

MidiTransport::MidiTransport() : in_(std::make_unique<RtMidiIn>()), out_(std::make_unique<RtMidiOut>()) {}

MidiTransport::~MidiTransport() = default;

std::vector<std::string> MidiTransport::listOutputPortNames() {
    RtMidiOut out;
    std::vector<std::string> names;
    for (unsigned int i = 0; i < out.getPortCount(); ++i) names.push_back(out.getPortName(i));
    return names;
}

std::vector<std::string> MidiTransport::listInputPortNames() {
    RtMidiIn in;
    std::vector<std::string> names;
    for (unsigned int i = 0; i < in.getPortCount(); ++i) names.push_back(in.getPortName(i));
    return names;
}

void MidiTransport::listPorts() {
    auto outNames = listOutputPortNames();
    auto inNames = listInputPortNames();

    std::cout << "Output ports:\n";
    for (unsigned int i = 0; i < outNames.size(); ++i) {
        std::cout << "  [" << i << "] " << outNames[i] << "\n";
    }
    std::cout << "Input ports:\n";
    for (unsigned int i = 0; i < inNames.size(); ++i) {
        std::cout << "  [" << i << "] " << inNames[i] << "\n";
    }
}

void MidiTransport::open(const std::string& nameSubstring, std::optional<unsigned int> portIndex) {
    unsigned int outIndex;
    unsigned int inIndex;

    if (portIndex.has_value()) {
        outIndex = *portIndex;
        inIndex = *portIndex;
        if (outIndex >= out_->getPortCount() || inIndex >= in_->getPortCount()) {
            throw std::runtime_error("--port-index " + std::to_string(*portIndex) +
                                      " is out of range (run 'electra list-ports' to see valid indices)");
        }
    } else {
        std::vector<std::string> outNames;
        for (unsigned int i = 0; i < out_->getPortCount(); ++i) outNames.push_back(out_->getPortName(i));
        std::vector<std::string> inNames;
        for (unsigned int i = 0; i < in_->getPortCount(); ++i) inNames.push_back(in_->getPortName(i));

        outIndex = findPortByName(outNames, nameSubstring, "output");
        inIndex = findPortByName(inNames, nameSubstring, "input");
    }

    out_->openPort(outIndex);
    in_->openPort(inIndex);
    // Don't ignore sysex; do ignore MIDI time code and active sensing noise.
    in_->ignoreTypes(false, true, true);
}

void MidiTransport::send(const std::vector<uint8_t>& bytes) {
    std::vector<unsigned char> msg(bytes.begin(), bytes.end());
    out_->sendMessage(&msg);
}

std::optional<std::vector<uint8_t>> MidiTransport::waitForReply(int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::vector<unsigned char> msg;

    while (std::chrono::steady_clock::now() < deadline) {
        msg.clear();
        in_->getMessage(&msg);
        if (!msg.empty() && msg.front() == sysex::SOX) {
            std::vector<uint8_t> bytes(msg.begin(), msg.end());
            if (sysex::parseResponse(bytes).has_value()) {
                return bytes;
            }
            // Not an Electra message (different manufacturer); keep waiting.
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return std::nullopt;
}
