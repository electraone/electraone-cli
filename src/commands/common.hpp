#pragma once

// Small shared helpers used across command group implementations: CLI11 and
// ArduinoJson are both header-only, so it's cheap for every commands/*.cpp
// file to pull this in directly.

#include <ArduinoJson.h>
#include <CLI/CLI.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "commands/runner.hpp"
#include "electraone/sysex.hpp"

namespace commands {

// Parses the "port1" / "port2" / "ctrl" selector used by Set Events MIDI
// Port and Set Logger MIDI Port into its wire byte.
inline uint8_t parsePortSelector(const std::string& value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return std::tolower(c); });
    if (v == "port1") return 0x00;
    if (v == "port2") return 0x01;
    if (v == "ctrl") return 0x02;
    throw std::runtime_error("invalid port selector '" + value + "': expected port1, port2, or ctrl");
}

// Adds --bank/-b and --slot/-s int options (default 0) to sub, writing into
// bank/slot and returning the two CLI::Option* (so callers can check
// ->count() to tell "explicitly passed" apart from "left at default", e.g.
// via optionalBankSlotParams). Electra bank/slot values are 7-bit.
inline std::pair<CLI::Option*, CLI::Option*> addBankSlot(CLI::App* sub, int& bank, int& slot,
                                                          bool required = false) {
    auto* b = sub->add_option("-b,--bank", bank, "Bank number (0-6)")->default_val(0);
    auto* s = sub->add_option("-s,--slot", slot, "Slot number (0-11)")->default_val(0);
    if (required) {
        b->required();
        s->required();
    }
    return {b, s};
}

inline std::vector<uint8_t> bankSlotParams(int bank, int slot) {
    return {static_cast<uint8_t>(bank), static_cast<uint8_t>(slot)};
}

// Several "get" queries take an *optional* bank/slot (omit both to mean
// "the current one"). Only emit the bytes if the user actually passed them.
inline std::vector<uint8_t> optionalBankSlotParams(CLI::Option* bankOpt, CLI::Option* slotOpt, int bank,
                                                    int slot) {
    if (bankOpt->count() > 0 || slotOpt->count() > 0) return bankSlotParams(bank, slot);
    return {};
}

// Adds the Subscribe Events flag options (--page, --control-set, ...,
// --all) to sub, writing into the given bool references. Shared by `events
// subscribe`/`events listen` and `logger listen` (which subscribes too, so
// non-log events print alongside log messages).
inline void addSubscribeFlags(CLI::App* sub, bool& page, bool& controlSet, bool& usbHost, bool& pots, bool& touch,
                               bool& button, bool& window, bool& all) {
    sub->add_flag("--page", page, "Subscribe to Page Switch events");
    sub->add_flag("--control-set", controlSet, "Subscribe to Control Set Switch events");
    sub->add_flag("--usb-host", usbHost, "Subscribe to USB Host Change events");
    sub->add_flag("--pots", pots, "Subscribe to Pots events");
    sub->add_flag("--touch", touch, "Subscribe to Touch events");
    sub->add_flag("--button", button, "Subscribe to Button events");
    sub->add_flag("--window", window, "Subscribe to Window events");
    sub->add_flag("--all", all, "Subscribe to all event types");
}

inline uint8_t subscribeFlagsToByte(bool page, bool controlSet, bool usbHost, bool pots, bool touch, bool button,
                                     bool window, bool all) {
    if (all) return 0x7F;
    uint8_t flags = 0;
    if (page) flags |= 1 << 0;
    if (controlSet) flags |= 1 << 1;
    if (usbHost) flags |= 1 << 2;
    if (pots) flags |= 1 << 3;
    if (touch) flags |= 1 << 4;
    if (button) flags |= 1 << 5;
    if (window) flags |= 1 << 6;
    return flags;
}

// Serializes an ArduinoJson document to a 7-bit ASCII byte vector suitable
// as a SysEx payload.
inline std::vector<uint8_t> jsonToBytes(const JsonDocument& doc) {
    std::string out;
    serializeJson(doc, out);
    return sysex::encodeAscii(out);
}

// Registers a global --port/--port-index/--out-port-index/--in-port-index/
// --timeout/--txn-id/-o/--raw option group on `app`, writing into ctx. Called
// once from main().
inline void addGlobalOptions(CLI::App& app, runner::Context& ctx) {
    app.add_option("--port", ctx.port,
                    "Substring to match against MIDI port names (default: CTRL)")
        ->default_val("CTRL");
    app.add_option("--port-index", ctx.portIndex,
                    "Use this MIDI port index directly for both output and input instead of "
                    "matching by name (see 'electra list-ports')");
    app.add_option("--out-port-index", ctx.outPortIndex,
                    "Use this index for the output port directly, overriding --port-index. Needed "
                    "when the output and input port lists are different lengths (e.g. on Windows, "
                    "where a software-only synth can show up as an extra output port), so the "
                    "correct output and input indices for the same device no longer match.");
    app.add_option("--in-port-index", ctx.inPortIndex,
                    "Use this index for the input port directly, overriding --port-index. See "
                    "--out-port-index.");
    app.add_option("--timeout", ctx.timeoutMs, "Reply timeout in milliseconds")
        ->default_val(3000);
    app.add_option("--txn-id", ctx.txnId, "Optional 14-bit transaction ID to attach to the request");
    app.add_option("-o,--output", ctx.outputFile, "Write the response to this file instead of stdout");
    app.add_flag("--raw", ctx.raw, "Print/write the raw response bytes as hex instead of decoding");
    app.add_flag("--pretty", ctx.pretty,
                 "Pretty-print JSON responses with indentation for human reading (non-JSON payloads, e.g. Lua "
                 "source, are printed unchanged)");
    app.add_flag("--human", ctx.human,
                 "Render JSON responses for human reading: objects as an indented tree preserving the JSON "
                 "structure, arrays as an `ls`-style listing (table for arrays of objects, a column grid for "
                 "arrays of scalars). Takes precedence over --pretty; -h is unavailable, it's already --help.");
}

}  // namespace commands
