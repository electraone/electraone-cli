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

#include "electraone/sysex.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sysex
{

    std::array<uint8_t, 2> encode14bit(uint16_t value)
    {
        if (value > 0x3FFF) {
            throw std::runtime_error("value " + std::to_string(value)
                                     + " does not fit in 14 bits");
        }
        return { static_cast<uint8_t>(value & 0x7F),
                 static_cast<uint8_t>(value >> 7) };
    }

    uint16_t decode14bit(uint8_t lsb, uint8_t msb)
    {
        return static_cast<uint16_t>((msb & 0x7F) << 7 | (lsb & 0x7F));
    }

    std::array<uint8_t, 4> encode28bit(uint32_t value)
    {
        if (value > 0x0FFFFFFFu) {
            throw std::runtime_error("value " + std::to_string(value)
                                     + " does not fit in 28 bits");
        }
        return {
            static_cast<uint8_t>(value & 0x7F),
            static_cast<uint8_t>((value >> 7) & 0x7F),
            static_cast<uint8_t>((value >> 14) & 0x7F),
            static_cast<uint8_t>((value >> 21) & 0x7F),
        };
    }

    uint32_t decode28bit(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
    {
        return (static_cast<uint32_t>(b0 & 0x7F))
               | (static_cast<uint32_t>(b1 & 0x7F) << 7)
               | (static_cast<uint32_t>(b2 & 0x7F) << 14)
               | (static_cast<uint32_t>(b3 & 0x7F) << 21);
    }

    std::vector<uint8_t> encodeAscii(const std::string &text)
    {
        std::vector<uint8_t> out;
        out.reserve(text.size());
        for (unsigned char c : text) {
            if (c >= 0x80) {
                throw std::runtime_error("non-ASCII byte 0x" + [c] {
                    std::ostringstream os;
                    os << std::hex << static_cast<int>(c);
                    return os.str();
                }() + " is not allowed in a 7-bit Electra SysEx payload");
            }
            out.push_back(static_cast<uint8_t>(c));
        }
        return out;
    }

    std::vector<uint8_t> buildMessage(uint8_t category,
                                      uint8_t command,
                                      const std::vector<uint8_t> &params,
                                      std::optional<uint16_t> txnId)
    {
        std::vector<uint8_t> msg;
        msg.reserve(8 + params.size());
        msg.push_back(SOX);
        msg.insert(msg.end(), ELECTRA_MFR_ID.begin(), ELECTRA_MFR_ID.end());
        if (txnId.has_value()) {
            auto split = encode14bit(*txnId);
            msg.push_back(0x00);
            msg.push_back(split[0]);
            msg.push_back(split[1]);
        }
        msg.push_back(category);
        msg.push_back(command);
        msg.insert(msg.end(), params.begin(), params.end());
        msg.push_back(EOX);
        return msg;
    }

    std::string ParsedResponse::payloadAsText() const
    {
        return std::string(payload.begin(), payload.end());
    }

    std::optional<ParsedResponse> parseResponse(const std::vector<uint8_t> &raw)
    {
        if (raw.size() < 7 || raw.front() != SOX || raw.back() != EOX) {
            return std::nullopt;
        }
        if (raw[1] != ELECTRA_MFR_ID[0] || raw[2] != ELECTRA_MFR_ID[1]
            || raw[3] != ELECTRA_MFR_ID[2]) {
            return std::nullopt;
        }

        size_t idx = 4;
        ParsedResponse resp;

        // Optional leading transaction-id prefix: 0x00 lsb msb. No real
        // category byte is ever 0x00, so this is unambiguous.
        if (idx < raw.size() - 1 && raw[idx] == 0x00 && idx + 2 < raw.size()) {
            resp.transactionId = decode14bit(raw[idx + 1], raw[idx + 2]);
            idx += 3;
        }

        if (idx + 1 >= raw.size()) {
            return std::nullopt; // no room for category+command
        }
        resp.category = raw[idx++];
        resp.command = raw[idx++];
        resp.payload.assign(raw.begin() + static_cast<long>(idx),
                            raw.end() - 1);

        resp.isAck = (resp.category == 0x7E && resp.command == 0x01);
        resp.isNack = (resp.category == 0x7E && resp.command == 0x00);

        // ACK/NACK optionally echo the transaction id as trailing lsb/msb bytes
        // right after the command byte (separate from the leading-prefix form).
        if ((resp.isAck || resp.isNack) && resp.payload.size() >= 2) {
            resp.transactionId = decode14bit(resp.payload[0], resp.payload[1]);
            resp.payload.clear();
        }

        return resp;
    }

    std::string toHex(const std::vector<uint8_t> &bytes)
    {
        std::ostringstream os;
        for (size_t i = 0; i < bytes.size(); ++i) {
            if (i)
                os << ' ';
            os << std::hex << std::uppercase << std::setw(2)
               << std::setfill('0') << static_cast<int>(bytes[i]);
        }
        return os.str();
    }

    bool isReplyTo(const ParsedResponse &resp,
                   uint8_t requestCategory,
                   uint8_t requestCommand)
    {
        if (resp.isAck || resp.isNack)
            return true;
        if (requestCategory == 0x02 && resp.category == 0x01
            && resp.command == requestCommand)
            return true;
        return resp.category == requestCategory
               && resp.command == requestCommand;
    }
} // namespace sysex
