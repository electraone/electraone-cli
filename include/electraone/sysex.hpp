#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sysex {

constexpr uint8_t SOX = 0xF0;
constexpr uint8_t EOX = 0xF7;
constexpr std::array<uint8_t, 3> ELECTRA_MFR_ID = {0x00, 0x21, 0x45};

// Splits a 14-bit value into {lsb, msb}, each 0x00-0x7F.
std::array<uint8_t, 2> encode14bit(uint16_t value);

// Recombines a 14-bit value from {lsb, msb}.
uint16_t decode14bit(uint8_t lsb, uint8_t msb);

// Splits a 28-bit value into four 7-bit bytes (least-significant first), the
// scheme the File Transfer API uses for file sizes (Register Files) and byte
// counts (Report Progress).
std::array<uint8_t, 4> encode28bit(uint32_t value);

// Recombines a 28-bit value from four 7-bit bytes (least-significant first).
uint32_t decode28bit(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3);

// Encodes text as individual ASCII bytes. Throws std::runtime_error if any
// character is >= 0x80 (Electra SysEx payloads are restricted to 7-bit bytes).
std::vector<uint8_t> encodeAscii(const std::string& text);

// Builds a full F0..F7 message: F0 00 21 45 [00 id-lsb id-msb] category command params... F7
// txnId is optional; when present it is emitted as the 3-byte transaction-id
// prefix immediately after the manufacturer ID, per the documented convention.
std::vector<uint8_t> buildMessage(uint8_t category, uint8_t command,
                                   const std::vector<uint8_t>& params,
                                   std::optional<uint16_t> txnId = std::nullopt);

struct ParsedResponse {
    bool isAck = false;
    bool isNack = false;
    std::optional<uint16_t> transactionId;
    uint8_t category = 0;
    uint8_t command = 0;
    std::vector<uint8_t> payload;  // bytes after category/command, before F7 (or after ack/nack+txn id)

    std::string payloadAsText() const;
};

// Parses a raw response message (including F0/F7). Returns std::nullopt if
// the message isn't a well-formed Electra SysEx message (wrong length,
// missing F0/F7, or manufacturer ID mismatch).
std::optional<ParsedResponse> parseResponse(const std::vector<uint8_t>& raw);

// Renders a byte vector as space-separated uppercase hex, e.g. "F0 00 21 45".
std::string toHex(const std::vector<uint8_t>& bytes);

}  // namespace sysex
