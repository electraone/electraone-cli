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

#include <doctest/doctest.h>

#include <electraone/ElectraOneClient.hpp>

using electraone::describeEvent;
using electraone::Response;

namespace
{

    Response makeResponse(uint8_t category,
                          uint8_t command,
                          std::vector<uint8_t> payload = {})
    {
        Response r;
        r.category = category;
        r.command = command;
        r.payload = std::move(payload);
        return r;
    }

} // namespace

TEST_CASE("describeEvent: ACK/NACK")
{
    Response ack;
    ack.isAck = true;
    CHECK(describeEvent(ack) == "ACK");

    Response ackWithTxn;
    ackWithTxn.isAck = true;
    ackWithTxn.transactionId = 42;
    CHECK(describeEvent(ackWithTxn) == "ACK (txn 42)");

    Response nack;
    nack.isNack = true;
    CHECK(describeEvent(nack) == "NACK");
}

TEST_CASE("describeEvent: Preset Switch")
{
    CHECK(describeEvent(makeResponse(0x7E, 0x02, { 2, 5 }))
          == "Preset Switch: bank=2 slot=5");
}

TEST_CASE("describeEvent: list-change events need no payload")
{
    CHECK(describeEvent(makeResponse(0x7E, 0x03)) == "Snapshot List Change");
    CHECK(describeEvent(makeResponse(0x7E, 0x31)) == "Capture List Change");
    CHECK(describeEvent(makeResponse(0x7E, 0x05)) == "Preset List Change");
}

TEST_CASE("describeEvent: Pot Touch decodes the 14-bit control id")
{
    // control = 1042 -> lsb=18, msb=8 (see test_sysex.cpp)
    CHECK(describeEvent(makeResponse(0x7E, 0x0A, { 3, 18, 8, 1 }))
          == "Pot Touch: pot=3 control=1042 touched=1");
}

TEST_CASE("describeEvent: Page Switch and Control Set Switch")
{
    CHECK(describeEvent(makeResponse(0x7E, 0x06, { 4 }))
          == "Page Switch: page=4");
    CHECK(describeEvent(makeResponse(0x7E, 0x07, { 2 }))
          == "Control Set Switch: set=2");
}

TEST_CASE("describeEvent: Preset Bank Switch and USB Host Change are distinct")
{
    // The firmware's own ElectraCommand::Event enum has PresetBankSwitch=0x08
    // and UsbHostChange=0x09 as separate values (no collision) - the docs'
    // suggestion that they share byte 0x08 was wrong.
    CHECK(describeEvent(makeResponse(0x7E, 0x08, { 6 }))
          == "Preset Bank Switch: bank=6");
    CHECK(describeEvent(makeResponse(0x7E, 0x09, {})) == "USB Host Change");
}

TEST_CASE("describeEvent: Snapshot Bank Switch")
{
    CHECK(describeEvent(makeResponse(0x7E, 0x04, { 1 }))
          == "Snapshot Bank Switch: bank=1");
}

TEST_CASE("describeEvent: Progress Report decodes the 28-bit byte count")
{
    // 0x0FFFFFFF-style value built the same way test_sysex.cpp verifies encode28bit/decode28bit.
    CHECK(describeEvent(makeResponse(0x7E, 0x2D, { 0x7F, 0x7F, 0x7F, 0x07 }))
          == "Progress Report: "
                 + std::to_string(sysex::decode28bit(0x7F, 0x7F, 0x7F, 0x07))
                 + " bytes transferred");
}

TEST_CASE("describeEvent: Log Message")
{
    std::string text = "[1234] hello";
    Response r = makeResponse(
        0x7F, 0x00, std::vector<uint8_t>(text.begin(), text.end()));
    CHECK(describeEvent(r) == "Log: " + text);
}

TEST_CASE(
    "describeEvent: MIDI Learn Info reassembles the JSON that the single-byte header split")
{
    // Category 0x03 has no separate command byte in the wire format - the
    // "command" byte parseResponse peeled off is really the JSON's first
    // character, per the comment in event_decoder / ElectraOneClient.cpp.
    std::string json = "{\"port\":1}";
    Response r =
        makeResponse(0x03,
                     static_cast<uint8_t>(json[0]),
                     std::vector<uint8_t>(json.begin() + 1, json.end()));
    CHECK(describeEvent(r) == "MIDI Learn Info: " + json);
}

TEST_CASE("describeEvent: unknown category/command falls back to a hex dump")
{
    CHECK(describeEvent(makeResponse(0x11, 0x22, { 0xAA, 0xBB }))
          == "0x11/0x22 payload=AA BB");
}

TEST_CASE("EventFlags::all sets every flag")
{
    auto flags = electraone::EventFlags::all();
    CHECK(flags.page);
    CHECK(flags.controlSet);
    CHECK(flags.usbHost);
    CHECK(flags.pots);
    CHECK(flags.touch);
    CHECK(flags.button);
    CHECK(flags.window);
}
