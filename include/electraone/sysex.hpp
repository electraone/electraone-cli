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

/**
 * @file sysex.hpp
 *
 * @brief Low-level Electra One SysEx envelope: byte encode/decode helpers,
 * message building, and response parsing. Shared by the electraone CLI and
 * the electraone_api library - this is the layer both are built on top of.
 */

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sysex
{

    constexpr uint8_t SOX = 0xF0;
    constexpr uint8_t EOX = 0xF7;
    constexpr std::array<uint8_t, 3> ELECTRA_MFR_ID = { 0x00, 0x21, 0x45 };

    /**
     * @brief Splits a 14-bit value into {lsb, msb}, each 0x00-0x7F.
     * @param value Value to split; must fit in 14 bits.
     * @return {lsb, msb} pair.
     */
    std::array<uint8_t, 2> encode14bit(uint16_t value);

    /**
     * @brief Recombines a 14-bit value from {lsb, msb}.
     * @param lsb Least-significant 7 bits.
     * @param msb Most-significant 7 bits.
     * @return The recombined 14-bit value.
     */
    uint16_t decode14bit(uint8_t lsb, uint8_t msb);

    /**
     * @brief Splits a 28-bit value into four 7-bit bytes (least-significant
     * first), the scheme the File Transfer API uses for file sizes (Register
     * Files) and byte counts (Report Progress).
     * @param value Value to split; must fit in 28 bits.
     * @return Four bytes, least-significant first.
     */
    std::array<uint8_t, 4> encode28bit(uint32_t value);

    /**
     * @brief Recombines a 28-bit value from four 7-bit bytes
     * (least-significant first).
     * @param b0 Least-significant byte.
     * @param b1 Second byte.
     * @param b2 Third byte.
     * @param b3 Most-significant byte.
     * @return The recombined 28-bit value.
     */
    uint32_t decode28bit(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);

    /**
     * @brief Encodes text as individual ASCII bytes.
     * @param text Text to encode.
     * @return One byte per character, unchanged.
     * @throws std::runtime_error if any character is >= 0x80 (Electra SysEx
     * payloads are restricted to 7-bit bytes).
     */
    std::vector<uint8_t> encodeAscii(const std::string &text);

    /**
     * @brief Builds a full F0..F7 message: `F0 00 21 45 [00 id-lsb id-msb]
     * category command params... F7`.
     * @param category SysEx category byte.
     * @param command SysEx command byte.
     * @param params Payload bytes to append after the category/command.
     * @param txnId Optional 14-bit transaction ID; when present it is emitted
     * as the 3-byte transaction-id prefix immediately after the manufacturer
     * ID, per the documented convention.
     * @return The complete message, including the leading F0 and trailing F7.
     */
    std::vector<uint8_t>
        buildMessage(uint8_t category,
                     uint8_t command,
                     const std::vector<uint8_t> &params,
                     std::optional<uint16_t> txnId = std::nullopt);

    /// A decoded reply, event, or unsolicited message from the device.
    struct ParsedResponse {
        bool isAck = false;
        bool isNack = false;
        std::optional<uint16_t> transactionId;
        uint8_t category = 0;
        uint8_t command = 0;
        std::vector<uint8_t> payload;

        /// @brief Renders payload as text (e.g. a JSON string or Lua source).
        std::string payloadAsText() const;
    };

    /**
     * @brief Parses a raw response message (including F0/F7).
     * @param raw Complete raw message bytes.
     * @return The decoded response, or std::nullopt if the message isn't a
     * well-formed Electra SysEx message (wrong length, missing F0/F7, or
     * manufacturer ID mismatch).
     */
    std::optional<ParsedResponse>
        parseResponse(const std::vector<uint8_t> &raw);

    /**
     * @brief Renders a byte vector as space-separated uppercase hex, e.g.
     * "F0 00 21 45".
     * @param bytes Bytes to render.
     * @return The formatted hex string.
     */
    std::string toHex(const std::vector<uint8_t> &bytes);

    /**
     * @brief Checks whether resp looks like the actual reply to a request
     * built with buildMessage(requestCategory, requestCommand, ...), as
     * opposed to an unrelated unsolicited event (e.g. Preset List Change)
     * that happened to arrive first - the device doesn't guarantee ordering
     * between the two.
     *
     * @note
     * - ACK/NACK (category 0x7E) always counts, regardless of what was
     *   requested.
     * - A query request (category 0x02) gets its data reply back under
     *   category 0x01 with the *same command byte* - not an echo of 0x02.
     *   This is documented explicitly for Get Location Files ("0xF0 ... 0x02
     *   0x34 ..." request -> "0xF0 ... 0x01 0x34 ..." response) and confirmed
     *   against a real device for Get Electra Info (0x02 0x7F -> 0x01 0x7F).
     *   Getting this wrong makes every query falsely reject its own reply and
     *   wait out the full timeout instead.
     * - Anything else must echo the exact category/command sent (covers
     *   undocumented/`raw` requests where this mapping isn't known).
     *
     * @param resp Response to check.
     * @param requestCategory Category byte the request was sent with.
     * @param requestCommand Command byte the request was sent with.
     * @return True if resp is the reply to that request.
     */
    bool isReplyTo(const ParsedResponse &resp,
                   uint8_t requestCategory,
                   uint8_t requestCommand);
} // namespace sysex
