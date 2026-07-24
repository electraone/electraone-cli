#include <iostream>
#include <sstream>
#include <stdexcept>

#include "commands/command.hpp"
#include "commands/common.hpp"
#include "midi_transport.hpp"

namespace {

std::vector<uint8_t> parseHexBytes(const std::string& s) {
    std::vector<uint8_t> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        out.push_back(static_cast<uint8_t>(std::stoul(tok, nullptr, 16)));
    }
    return out;
}

}  // namespace

void registerRawCommands(CLI::App& app, runner::Context& ctx) {
    static int category = -1, command = -1;
    static std::string bytesHex, payloadAscii, payloadFile;

    auto* sub = app.add_subcommand(
        "raw", "Send an arbitrary SysEx message - escape hatch for anything not fully wired up, or where the "
               "docs are ambiguous (see the capture command group)");
    sub->add_option("--category", category, "Category byte (hex or decimal), e.g. 0x02");
    sub->add_option("--command", command, "Command byte (hex or decimal), e.g. 0x01");
    sub->add_option("--payload-ascii", payloadAscii, "Payload text, ASCII-encoded");
    sub->add_option("--payload-file", payloadFile, "Read the payload from this file (use - for stdin)");
    sub->add_option("--bytes", bytesHex,
                     "Send this exact message instead of building one from --category/--command/--payload-*, as "
                     "space-separated hex bytes including F0...F7");

    sub->callback([&ctx] {
        std::vector<uint8_t> msg;
        if (!bytesHex.empty()) {
            msg = parseHexBytes(bytesHex);
        } else {
            if (category < 0 || category > 0xFF || command < 0 || command > 0xFF) {
                throw std::runtime_error(
                    "--category and --command are required (0x00-0xFF) unless --bytes is given");
            }
            std::vector<uint8_t> payload;
            if (!payloadFile.empty()) {
                payload = sysex::encodeAscii(runner::readFileOrStdin(payloadFile));
            } else if (!payloadAscii.empty()) {
                payload = sysex::encodeAscii(payloadAscii);
            }
            msg = sysex::buildMessage(static_cast<uint8_t>(category), static_cast<uint8_t>(command), payload,
                                       ctx.txnId);
        }

        MidiTransport transport;
        transport.open(ctx.port, ctx.portIndex);
        transport.send(msg);

        auto raw = transport.waitForReply(ctx.timeoutMs);
        if (!raw.has_value()) {
            std::cerr << "error: timed out waiting for a reply\n";
            runner::exitCode = 2;
            return;
        }
        if (ctx.raw) {
            runner::writeOutput(ctx, sysex::toHex(*raw));
            runner::exitCode = 0;
            return;
        }
        auto parsed = sysex::parseResponse(*raw);
        if (!parsed.has_value()) {
            runner::writeOutput(ctx, sysex::toHex(*raw));
            runner::exitCode = 0;
            return;
        }
        if (parsed->isAck) {
            std::cout << "ACK";
            if (parsed->transactionId.has_value()) std::cout << " (txn " << *parsed->transactionId << ")";
            std::cout << "\n";
            runner::exitCode = 0;
            return;
        }
        if (parsed->isNack) {
            std::cerr << "NACK\n";
            runner::exitCode = 1;
            return;
        }
        runner::writeOutput(ctx, runner::formatPayload(ctx, parsed->payloadAsText()));
        runner::exitCode = 0;
    });
}
