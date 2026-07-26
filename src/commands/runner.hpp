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
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "electraone/sysex.hpp"

/**
 * @file runner.hpp
 *
 * @brief Shared plumbing used by every command group: turns a (category,
 * command, params) triple into a sent SysEx message, waits for the Electra's
 * reply, and prints/writes the decoded result.
 *
 * @note Every command's exit code ends up in runner::exitCode, which main()
 * returns after app.parse() completes (CLI11 subcommand callbacks are
 * void(), so they can't return a value directly).
 */
namespace runner
{

    /// @brief Global CLI options shared by every command (see
    /// commands::addGlobalOptions).
    struct Context {
        std::string port = "CTRL";
        std::optional<unsigned int>
            portIndex; ///< Applies to both output and input port lists.
        std::optional<unsigned int>
            outPortIndex; ///< Overrides portIndex for the output port only.
        std::optional<unsigned int>
            inPortIndex; ///< Overrides portIndex for the input port only.
        int timeoutMs = 3000;
        std::optional<uint16_t> txnId;
        std::string outputFile; ///< Empty means stdout.
        bool raw = false; ///< Dump raw response hex instead of decoding.
        bool pretty = false; ///< Pretty-print JSON payloads for human reading.
        /// Render JSON as a tree (objects) / ls-style listing (arrays).
        bool human = false;

        /**
         * @brief Resolves the effective output port index.
         *
         * The output/input port lists aren't always the same length (e.g. on
         * Windows, a software-only synth can appear as an extra output port
         * with no matching input, shifting every real device's output index
         * out of alignment with its input index) - so
         * --out-port-index/--in-port-index take precedence over the
         * symmetric --port-index when set.
         */
        std::optional<unsigned int> resolvedOutPortIndex() const
        {
            return outPortIndex ? outPortIndex : portIndex;
        }
        /// @copybrief resolvedOutPortIndex
        std::optional<unsigned int> resolvedInPortIndex() const
        {
            return inPortIndex ? inPortIndex : portIndex;
        }
    };

    /// @brief Exit code of the most recently executed command: 0
    /// success/ACK, 1 NACK, 2 timeout or transport/protocol error.
    extern int exitCode;

    /**
     * @brief Sends the message and waits for a reply, treating it as a
     * query (data comes back instead of a plain ACK).
     *
     * Sets runner::exitCode and prints the decoded payload.
     *
     * @param ctx Global CLI context (port, timeout, output options).
     * @param category SysEx category byte.
     * @param command SysEx command byte.
     * @param params Payload bytes.
     */
    void runQuery(const Context &ctx,
                  uint8_t category,
                  uint8_t command,
                  const std::vector<uint8_t> &params);

    /**
     * @brief Sends the message and waits for a reply, treating it as an
     * ACK/NACK-style action (uploads, removes, runtime commands).
     * @param ctx Global CLI context (port, timeout, output options).
     * @param category SysEx category byte.
     * @param command SysEx command byte.
     * @param params Payload bytes.
     */
    void runAction(const Context &ctx,
                   uint8_t category,
                   uint8_t command,
                   const std::vector<uint8_t> &params);

    /**
     * @brief Like runAction, but for requests that may receive interleaved
     * unsolicited messages before their real ACK/NACK - namely Transfer
     * Chunks, which the device accompanies with Report Progress events.
     *
     * Keeps waiting (resetting the timeout window each time *any* message
     * arrives) until an ACK/NACK shows up, invoking onOther for every
     * non-ACK/NACK message seen along the way. Sets runner::exitCode like
     * runAction.
     *
     * @param ctx Global CLI context (port, timeout, output options).
     * @param category SysEx category byte.
     * @param command SysEx command byte.
     * @param params Payload bytes.
     * @param onOther Invoked for every non-ACK/NACK message seen while
     * waiting.
     */
    void runActionAwaitingAck(
        const Context &ctx,
        uint8_t category,
        uint8_t command,
        const std::vector<uint8_t> &params,
        const std::function<void(const sysex::ParsedResponse &)> &onOther);

    /// @brief Reads an entire file, or stdin if path == "-".
    std::string readFileOrStdin(const std::string &path);

    /// @brief Writes text to ctx.outputFile, or stdout if it's empty.
    void writeOutput(const Context &ctx, const std::string &text);

    /**
     * @brief Pretty-prints a payload for human reading if requested.
     * @param ctx Consulted for ctx.pretty.
     * @param text Payload text.
     * @return If ctx.pretty is set and text parses as JSON, text
     * re-serialized with indentation; otherwise text unchanged (covers
     * non-JSON payloads like Lua source, and ctx.pretty == false).
     */
    std::string formatPayload(const Context &ctx, const std::string &text);

    /**
     * @brief Opens a transport, sends each of initialMessages in turn (e.g.
     * a Subscribe Events or Control Logger Output + Set Logger MIDI Port
     * request), checking none is NACKed, then polls for incoming Electra
     * messages and invokes onEvent for each one until durationSec elapses
     * (or forever if durationSec <= 0).
     *
     * @note Runs until killed (Ctrl+C) when unbounded. Intended for
     * `... listen` subcommands.
     *
     * @param ctx Global CLI context (port, timeout options).
     * @param initialMessages Messages to send (in order) before listening.
     * @param durationSec Stop after this many seconds; <= 0 runs forever.
     * @param onEvent Invoked for each incoming message.
     */
    void listen(
        const Context &ctx,
        const std::vector<std::vector<uint8_t>> &initialMessages,
        int durationSec,
        const std::function<void(const sysex::ParsedResponse &)> &onEvent);

} // namespace runner
