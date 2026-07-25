#include "commands/runner.hpp"

#include <ArduinoJson.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

#include "commands/human_format.hpp"
#include "midi_transport.hpp"

namespace runner
{

    int exitCode = 0;

    namespace
    {

        std::optional<sysex::ParsedResponse>
            sendAndReceive(const Context &ctx,
                           uint8_t category,
                           uint8_t command,
                           const std::vector<uint8_t> &params,
                           std::vector<uint8_t> *rawOut)
        {
            MidiTransport transport;
            transport.open(ctx.port,
                           ctx.resolvedOutPortIndex(),
                           ctx.resolvedInPortIndex());

            auto msg =
                sysex::buildMessage(category, command, params, ctx.txnId);
            transport.send(msg);

            // Many commands trigger an unsolicited notification event alongside
            // their real reply (e.g. Upload Preset also emits a Preset List Change
            // event) - the device doesn't guarantee which arrives first. Skip past
            // anything that isn't actually our reply (see sysex::isReplyTo) so an
            // unrelated event can't be mistaken for it.
            while (true) {
                auto raw = transport.waitForReply(ctx.timeoutMs);
                if (!raw.has_value()) {
                    return std::nullopt;
                }
                auto resp = sysex::parseResponse(*raw);
                if (!resp.has_value())
                    continue; // shouldn't happen; waitForReply already filters

                if (sysex::isReplyTo(*resp, category, command)) {
                    if (rawOut)
                        *rawOut = *raw;
                    return resp;
                }
                // Unrelated unsolicited event - keep waiting for the real reply.
            }
        }

    } // namespace

    void runQuery(const Context &ctx,
                  uint8_t category,
                  uint8_t command,
                  const std::vector<uint8_t> &params)
    {
        std::vector<uint8_t> raw;
        auto resp = sendAndReceive(ctx, category, command, params, &raw);

        if (!resp.has_value()) {
            std::cerr
                << "error: timed out waiting for a reply from the device\n";
            exitCode = 2;
            return;
        }
        if (ctx.raw) {
            writeOutput(ctx, sysex::toHex(raw));
            exitCode = resp->isNack ? 1 : 0;
            return;
        }
        if (resp->isNack) {
            std::cerr << "NACK";
            if (resp->transactionId.has_value())
                std::cerr << " (txn " << *resp->transactionId << ")";
            std::cerr << "\n";
            exitCode = 1;
            return;
        }
        writeOutput(ctx, formatPayload(ctx, resp->payloadAsText()));
        exitCode = 0;
    }

    void runAction(const Context &ctx,
                   uint8_t category,
                   uint8_t command,
                   const std::vector<uint8_t> &params)
    {
        std::vector<uint8_t> raw;
        auto resp = sendAndReceive(ctx, category, command, params, &raw);

        if (!resp.has_value()) {
            std::cerr
                << "error: timed out waiting for a reply from the device\n";
            exitCode = 2;
            return;
        }
        if (ctx.raw) {
            writeOutput(ctx, sysex::toHex(raw));
            exitCode = resp->isNack ? 1 : 0;
            return;
        }
        if (resp->isNack) {
            std::cerr << "NACK";
            if (resp->transactionId.has_value())
                std::cerr << " (txn " << *resp->transactionId << ")";
            std::cerr << "\n";
            exitCode = 1;
            return;
        }
        if (!resp->isAck) {
            // Some replies (e.g. Preset Switch) aren't a strict ACK but still
            // indicate success; surface the raw payload so nothing is silently lost.
            std::cout << "OK (unexpected reply category=0x" << std::hex
                      << int(resp->category) << " command=0x"
                      << int(resp->command) << std::dec << ")\n";
            if (!resp->payload.empty())
                std::cout << formatPayload(ctx, resp->payloadAsText()) << "\n";
            exitCode = 0;
            return;
        }
        std::cout << "OK";
        if (resp->transactionId.has_value())
            std::cout << " (txn " << *resp->transactionId << ")";
        std::cout << "\n";
        exitCode = 0;
    }

    void runActionAwaitingAck(
        const Context &ctx,
        uint8_t category,
        uint8_t command,
        const std::vector<uint8_t> &params,
        const std::function<void(const sysex::ParsedResponse &)> &onOther)
    {
        MidiTransport transport;
        transport.open(
            ctx.port, ctx.resolvedOutPortIndex(), ctx.resolvedInPortIndex());

        auto msg = sysex::buildMessage(category, command, params, ctx.txnId);
        transport.send(msg);

        while (true) {
            auto raw = transport.waitForReply(ctx.timeoutMs);
            if (!raw.has_value()) {
                std::cerr
                    << "error: timed out waiting for a reply from the device\n";
                exitCode = 2;
                return;
            }
            auto resp = sysex::parseResponse(*raw);
            if (!resp.has_value())
                continue; // shouldn't happen; waitForReply already filters

            if (resp->isAck) {
                exitCode = 0;
                return;
            }
            if (resp->isNack) {
                std::cerr << "NACK";
                if (resp->transactionId.has_value())
                    std::cerr << " (txn " << *resp->transactionId << ")";
                std::cerr << "\n";
                exitCode = 1;
                return;
            }
            if (onOther)
                onOther(*resp);
        }
    }

    std::string readFileOrStdin(const std::string &path)
    {
        if (path == "-") {
            std::ostringstream ss;
            ss << std::cin.rdbuf();
            return ss.str();
        }
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            throw std::runtime_error("could not open file: " + path);
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    std::string formatPayload(const Context &ctx, const std::string &text)
    {
        if (ctx.human)
            return commands::formatHumanReadable(text);
        if (!ctx.pretty)
            return text;
        JsonDocument doc;
        if (deserializeJson(doc, text))
            return text; // not JSON (or malformed) - leave as-is
        std::string pretty;
        serializeJsonPretty(doc, pretty);
        return pretty;
    }

    void writeOutput(const Context &ctx, const std::string &text)
    {
        if (ctx.outputFile.empty()) {
            std::cout << text;
            if (text.empty() || text.back() != '\n')
                std::cout << "\n";
            return;
        }
        std::ofstream f(ctx.outputFile, std::ios::binary);
        if (!f) {
            throw std::runtime_error("could not open output file: "
                                     + ctx.outputFile);
        }
        f << text;
    }

    void listen(
        const Context &ctx,
        const std::vector<std::vector<uint8_t>> &initialMessages,
        int durationSec,
        const std::function<void(const sysex::ParsedResponse &)> &onEvent)
    {
        MidiTransport transport;
        transport.open(
            ctx.port, ctx.resolvedOutPortIndex(), ctx.resolvedInPortIndex());

        using clock = std::chrono::steady_clock;
        std::optional<clock::time_point> deadline;
        if (durationSec > 0)
            deadline = clock::now() + std::chrono::seconds(durationSec);

        // `listen` commands are meant to run until Ctrl+C or --duration, not
        // ctx.timeoutMs (that's for quick one-shot request/reply commands) - so
        // both the initial setup handshake and the event loop below poll in
        // short increments, bounded only by --duration if one was given.
        // Without --duration this waits indefinitely, which is the point.
        auto waitForNext = [&]() -> std::optional<std::vector<uint8_t>> {
            while (!deadline.has_value() || clock::now() < *deadline) {
                auto msg = transport.waitForReply(200);
                if (msg.has_value())
                    return msg;
            }
            return std::nullopt;
        };

        if (!initialMessages.empty())
            std::cout << "Connecting...\n";
        for (const auto &initialMessage : initialMessages) {
            transport.send(initialMessage);
            auto ack = waitForNext();
            if (!ack.has_value()) {
                std::cerr
                    << "error: --duration elapsed before getting a reply to the initial request\n";
                exitCode = 2;
                return;
            }
            auto parsed = sysex::parseResponse(*ack);
            if (parsed.has_value() && parsed->isNack) {
                std::cerr << "NACK on initial request\n";
                exitCode = 1;
                return;
            }
        }

        std::cout << "Listening";
        if (durationSec > 0)
            std::cout << " for " << durationSec << "s";
        std::cout << " (Ctrl+C to stop)...\n";

        while (auto msg = waitForNext()) {
            auto parsed = sysex::parseResponse(*msg);
            if (parsed.has_value())
                onEvent(*parsed);
        }
        exitCode = 0;
    }

} // namespace runner
