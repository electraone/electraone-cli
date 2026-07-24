#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "electraone/sysex.hpp"

// Shared plumbing used by every command group: turns a (category, command,
// params) triple into a sent SysEx message, waits for the Electra's reply,
// and prints/writes the decoded result. Every command's exit code ends up in
// runner::exitCode, which main() returns after app.parse() completes (CLI11
// subcommand callbacks are void(), so they can't return a value directly).
namespace runner {

struct Context {
    std::string port = "CTRL";
    std::optional<unsigned int> portIndex;     // applies to both output and input port lists
    std::optional<unsigned int> outPortIndex;  // overrides portIndex for the output port only
    std::optional<unsigned int> inPortIndex;   // overrides portIndex for the input port only
    int timeoutMs = 3000;
    std::optional<uint16_t> txnId;
    std::string outputFile;  // empty => stdout
    bool raw = false;        // dump raw response hex instead of decoding
    bool pretty = false;     // pretty-print JSON payloads for human reading
    bool human = false;      // render JSON as a tree (objects) / ls-style listing (arrays)

    // The output/input port lists aren't always the same length (e.g. on
    // Windows, a software-only synth can appear as an extra output port with
    // no matching input, shifting every real device's output index out of
    // alignment with its input index) - so --out-port-index/--in-port-index
    // take precedence over the symmetric --port-index when set.
    std::optional<unsigned int> resolvedOutPortIndex() const { return outPortIndex ? outPortIndex : portIndex; }
    std::optional<unsigned int> resolvedInPortIndex() const { return inPortIndex ? inPortIndex : portIndex; }
};

// Exit code of the most recently executed command: 0 success/ACK, 1 NACK,
// 2 timeout or transport/protocol error.
extern int exitCode;

// Sends the message and waits for a reply. Sets runner::exitCode and prints
// the decoded payload (query-style commands: data comes back instead of a
// plain ACK).
void runQuery(const Context& ctx, uint8_t category, uint8_t command,
              const std::vector<uint8_t>& params);

// Sends the message and waits for a reply, treating it as an ACK/NACK-style
// action (uploads, removes, runtime commands).
void runAction(const Context& ctx, uint8_t category, uint8_t command,
               const std::vector<uint8_t>& params);

// Like runAction, but for requests that may receive interleaved unsolicited
// messages before their real ACK/NACK - namely Transfer Chunks, which the
// device accompanies with Report Progress events. Keeps waiting (resetting
// the timeout window each time *any* message arrives) until an ACK/NACK
// shows up, invoking onOther for every non-ACK/NACK message seen along the
// way. Sets runner::exitCode like runAction.
void runActionAwaitingAck(const Context& ctx, uint8_t category, uint8_t command,
                           const std::vector<uint8_t>& params,
                           const std::function<void(const sysex::ParsedResponse&)>& onOther);

// Reads an entire file, or stdin if path == "-".
std::string readFileOrStdin(const std::string& path);

// Writes text to ctx.outputFile, or stdout if it's empty.
void writeOutput(const Context& ctx, const std::string& text);

// If ctx.pretty is set and text parses as JSON, returns it re-serialized
// with indentation for human reading; otherwise returns text unchanged
// (covers non-JSON payloads like Lua source, and ctx.pretty == false).
std::string formatPayload(const Context& ctx, const std::string& text);

// Opens a transport, sends each of initialMessages in turn (e.g. a Subscribe
// Events or Control Logger Output + Set Logger MIDI Port request), checking
// none is NACKed, then polls for incoming Electra messages and invokes
// onEvent for each one until durationSec elapses (or forever if durationSec
// <= 0). Runs until killed (Ctrl+C) when unbounded. Intended for `... listen`
// subcommands.
void listen(const Context& ctx, const std::vector<std::vector<uint8_t>>& initialMessages, int durationSec,
            const std::function<void(const sysex::ParsedResponse&)>& onEvent);

}  // namespace runner
