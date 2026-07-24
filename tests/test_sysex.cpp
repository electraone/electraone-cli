#include <doctest/doctest.h>

#include <electraone/sysex.hpp>

using namespace sysex;

TEST_CASE("encode14bit/decode14bit round-trip") {
    for (uint16_t v : {static_cast<uint16_t>(0), static_cast<uint16_t>(1), static_cast<uint16_t>(0x7F),
                        static_cast<uint16_t>(0x80), static_cast<uint16_t>(0x3FFF)}) {
        auto bytes = encode14bit(v);
        CHECK(bytes[0] <= 0x7F);
        CHECK(bytes[1] <= 0x7F);
        CHECK(decode14bit(bytes[0], bytes[1]) == v);
    }
}

TEST_CASE("encode14bit matches the documented lsb/msb split") {
    // 1042 = 0b10000010010 -> lsb = 1042 & 0x7F = 18, msb = 1042 >> 7 = 8
    auto bytes = encode14bit(1042);
    CHECK(bytes[0] == 18);
    CHECK(bytes[1] == 8);
}

TEST_CASE("encode14bit rejects values that don't fit in 14 bits") {
    CHECK_THROWS_AS(encode14bit(0x4000), std::runtime_error);
}

TEST_CASE("encode28bit/decode28bit round-trip") {
    for (uint32_t v : {0u, 1u, 0x7Fu, 0x80u, 0x0FFFFFFFu}) {
        auto bytes = encode28bit(v);
        for (auto b : bytes) CHECK(b <= 0x7F);
        CHECK(decode28bit(bytes[0], bytes[1], bytes[2], bytes[3]) == v);
    }
}

TEST_CASE("encode28bit rejects values that don't fit in 28 bits") {
    CHECK_THROWS_AS(encode28bit(0x10000000u), std::runtime_error);
}

TEST_CASE("encodeAscii passes through valid 7-bit text unchanged") {
    auto bytes = encodeAscii("hello {}");
    std::string back(bytes.begin(), bytes.end());
    CHECK(back == "hello {}");
}

TEST_CASE("encodeAscii rejects bytes >= 0x80") {
    std::string withHighBit = "ok\xFFnot ok";
    CHECK_THROWS_AS(encodeAscii(withHighBit), std::runtime_error);
}

TEST_CASE("buildMessage: Get Electra Info matches the documented byte sequence") {
    // F0 00 21 45 02 7F F7, per docs.electra.one/developers/midiimplementation.html
    auto msg = buildMessage(0x02, 0x7F, {});
    std::vector<uint8_t> expected = {0xF0, 0x00, 0x21, 0x45, 0x02, 0x7F, 0xF7};
    CHECK(msg == expected);
}

TEST_CASE("buildMessage: with params") {
    auto msg = buildMessage(0x05, 0x01, {0x02, 0x03});  // Remove Preset, bank=2 slot=3
    std::vector<uint8_t> expected = {0xF0, 0x00, 0x21, 0x45, 0x05, 0x01, 0x02, 0x03, 0xF7};
    CHECK(msg == expected);
}

TEST_CASE("buildMessage: optional transaction id is emitted as 00 lsb msb right after the manufacturer id") {
    auto msg = buildMessage(0x02, 0x7F, {}, 1042);
    std::vector<uint8_t> expected = {0xF0, 0x00, 0x21, 0x45, 0x00, 18, 8, 0x02, 0x7F, 0xF7};
    CHECK(msg == expected);
}

TEST_CASE("parseResponse: decodes an ACK") {
    std::vector<uint8_t> raw = {0xF0, 0x00, 0x21, 0x45, 0x7E, 0x01, 0xF7};
    auto resp = parseResponse(raw);
    REQUIRE(resp.has_value());
    CHECK(resp->isAck);
    CHECK_FALSE(resp->isNack);
    CHECK(resp->payload.empty());
    CHECK_FALSE(resp->transactionId.has_value());
}

TEST_CASE("parseResponse: decodes a NACK with an echoed transaction id") {
    std::vector<uint8_t> raw = {0xF0, 0x00, 0x21, 0x45, 0x7E, 0x00, 18, 8, 0xF7};
    auto resp = parseResponse(raw);
    REQUIRE(resp.has_value());
    CHECK(resp->isNack);
    REQUIRE(resp->transactionId.has_value());
    CHECK(*resp->transactionId == 1042);
    CHECK(resp->payload.empty());
}

TEST_CASE("parseResponse: decodes a data reply and payloadAsText") {
    // F0 00 21 45 02 7F {"a":1} F7
    std::vector<uint8_t> raw = {0xF0, 0x00, 0x21, 0x45, 0x02, 0x7F};
    std::string json = "{\"a\":1}";
    raw.insert(raw.end(), json.begin(), json.end());
    raw.push_back(0xF7);

    auto resp = parseResponse(raw);
    REQUIRE(resp.has_value());
    CHECK_FALSE(resp->isAck);
    CHECK_FALSE(resp->isNack);
    CHECK(resp->category == 0x02);
    CHECK(resp->command == 0x7F);
    CHECK(resp->payloadAsText() == json);
}

TEST_CASE("parseResponse: decodes the leading transaction-id prefix on a data reply") {
    std::vector<uint8_t> raw = {0xF0, 0x00, 0x21, 0x45, 0x00, 18, 8, 0x02, 0x7F, 'O', 'K', 0xF7};
    auto resp = parseResponse(raw);
    REQUIRE(resp.has_value());
    REQUIRE(resp->transactionId.has_value());
    CHECK(*resp->transactionId == 1042);
    CHECK(resp->category == 0x02);
    CHECK(resp->command == 0x7F);
    CHECK(resp->payloadAsText() == "OK");
}

TEST_CASE("parseResponse: rejects malformed messages") {
    CHECK_FALSE(parseResponse({}).has_value());
    CHECK_FALSE(parseResponse({0xF0, 0x00, 0x21, 0x45, 0x02, 0x7F}).has_value());              // missing F7
    CHECK_FALSE(parseResponse({0x00, 0x21, 0x45, 0x02, 0x7F, 0xF7}).has_value());               // missing F0
    CHECK_FALSE(parseResponse({0xF0, 0x00, 0x21, 0x46, 0x02, 0x7F, 0xF7}).has_value());         // wrong mfr id
    CHECK_FALSE(parseResponse({0xF0, 0x00, 0x21, 0x45, 0xF7}).has_value());                     // too short
}

TEST_CASE("toHex formats bytes as space-separated uppercase hex") {
    CHECK(toHex({0xF0, 0x00, 0x21, 0x45}) == "F0 00 21 45");
    CHECK(toHex({}).empty());
}
