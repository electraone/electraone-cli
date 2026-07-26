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
 * @file electra_command.hpp
 *
 * @brief Named SysEx category/command byte constants, transcribed from the
 * firmware's own ElectraCommand.h (Type/Object/Event enums) so every SysEx
 * message this codebase builds can be traced back to that source instead of
 * a bare hex literal. Internal header (not part of the public electraone_api
 * surface) - used by both the CLI and the library's .cpp files.
 *
 * Fully-qualified access mirrors the firmware exactly, e.g.
 * ElectraCommand::Type::FileRequest, ElectraCommand::Object::FilePreset. This
 * uses plain constexpr uint8_t constants rather than the firmware's `enum
 * class` so they can be passed directly as the uint8_t category/command
 * arguments used throughout this codebase (sysex::buildMessage,
 * runner::runQuery/runAction, Client::send, ...) without a cast at every call
 * site - a practical choice for this port, not a claim that the firmware
 * itself defines them this way.
 */

#pragma once

#include <cstdint>

namespace ElectraCommand
{

    // The first byte after the manufacturer ID: what kind of operation this is.
    namespace Type
    {
        constexpr uint8_t FileUpload = 0x01; // File transfer
        constexpr uint8_t FileRequest =
            0x02; // (Fetch) Request in the application
        constexpr uint8_t MidiLearnSwitch = 0x03;
        constexpr uint8_t Update = 0x04;
        constexpr uint8_t Remove = 0x05;
        constexpr uint8_t Swap = 0x06;
        constexpr uint8_t Create = 0x07;
        constexpr uint8_t Execute = 0x08;
        constexpr uint8_t Switch = 0x09;
        constexpr uint8_t UpdateRuntime = 0x14;
        constexpr uint8_t DataContainer = 0x7D;
        constexpr uint8_t Event = 0x7E;
        constexpr uint8_t SystemCall = 0x7F;
        constexpr uint8_t Unknown = 0x00;
    } // namespace Type

    // The second byte: what the operation applies to. Values are reused across
    // different Type categories (the firmware's own Object enum does this too -
    // e.g. FileConfig/0x02 as a command byte coincides numerically with
    // Type::FileRequest/0x02 as a category byte; position, not value, says which
    // is which).
    namespace Object
    {
        constexpr uint8_t FilePresetLegacy = 0x00;
        constexpr uint8_t FilePreset = 0x01;
        constexpr uint8_t FileConfig = 0x02;
        constexpr uint8_t FileSnapshot = 0x03;
        constexpr uint8_t PresetList = 0x04;
        constexpr uint8_t SnapshotList = 0x05;
        constexpr uint8_t SnapshotInfo = 0x06;
        constexpr uint8_t Control = 0x07;
        constexpr uint8_t PresetSlot = 0x08;
        constexpr uint8_t SnapshotSlot = 0x09;
        constexpr uint8_t Page = 0x0A;
        constexpr uint8_t ControlSet = 0x0B;
        constexpr uint8_t FileLua = 0x0C;
        constexpr uint8_t Function = 0x0D;
        constexpr uint8_t Value = 0x0E;
        constexpr uint8_t DeviceList = 0x0F;
        constexpr uint8_t UsbHostList = 0x10;
        constexpr uint8_t Performance = 0x11;
        constexpr uint8_t Datafile = 0x12;
        constexpr uint8_t FileFirmware = 0x21;
        constexpr uint8_t FilePackage = 0x22;
        constexpr uint8_t FileUi = 0x23;
        constexpr uint8_t FileStagedCache = 0x2D;
        constexpr uint8_t FileStagedHeader = 0x2E;
        constexpr uint8_t FileStagedChunk = 0x2F;
        constexpr uint8_t FileCapture = 0x30;
        constexpr uint8_t CaptureList = 0x31;
        constexpr uint8_t CaptureInfo = 0x32;
        constexpr uint8_t CaptureSlot = 0x33;
        constexpr uint8_t Location = 0x34;
        constexpr uint8_t Trace = 0x40;
        constexpr uint8_t ParameterMap = 0x41;
        constexpr uint8_t Screenshot = 0x76;
        constexpr uint8_t StatusBar = 0x77;
        constexpr uint8_t Reboot = 0x78;
        constexpr uint8_t EventSubscription = 0x79;
        constexpr uint8_t Window = 0x7A;
        constexpr uint8_t ControlPort = 0x7B;
        constexpr uint8_t AppInfo = 0x7C;
        constexpr uint8_t Logger = 0x7D;
        constexpr uint8_t RuntimeInfo = 0x7E;
        constexpr uint8_t ElectraInfo = 0x7F;
        constexpr uint8_t Unknown = 0x3F;
        constexpr uint8_t Disable = 0x02;
        constexpr uint8_t Enable = 0x01;

        // MidiLearnOn/MidiLearnOff intentionally do NOT reuse the firmware
        // enum's own values (MidiLearnOn=0x00, MidiLearnOff=0x01) here: per
        // ElectraCommand::set()'s special case for MidiLearnSwitch
        // ("TODO: ugly fix because the MIDI learn sysex is not ok"), the
        // firmware actually decides on/off from the *wire byte* itself
        // (objectByte == 1 -> MidiLearnOn), not from those enum values. These
        // constants reflect that wire behaviour, which is what Client::setMidiLearn
        // and `midi-learn enable/disable` need.
        constexpr uint8_t MidiLearnOn = 0x01;
        constexpr uint8_t MidiLearnOff = 0x00;
    } // namespace Object

    // The Event byte for category Type::Event (0x7E) messages - both the two the
    // firmware's SysexApi::processEvent() itself dispatches (SnapshotBankSwitch,
    // CaptureBankSwitch) and the ones it emits as unsolicited notifications.
    namespace Event
    {
        constexpr uint8_t Nack = 0x00;
        constexpr uint8_t Ack = 0x01;
        constexpr uint8_t PresetSwitch = 0x02;
        constexpr uint8_t SnapshotChange = 0x03;
        constexpr uint8_t SnapshotBankSwitch = 0x04;
        constexpr uint8_t PresetListChange = 0x05;
        constexpr uint8_t PageSwitch = 0x06;
        constexpr uint8_t ControlSetSwitch = 0x07;
        constexpr uint8_t PresetBankSwitch = 0x08;
        constexpr uint8_t UsbHostChange = 0x09;
        constexpr uint8_t PotTouch = 0x0A;
        constexpr uint8_t CaptureChange = 0x0B;
        constexpr uint8_t CaptureBankSwitch = 0x0C;
        constexpr uint8_t Unknown = 0x7F;
    } // namespace Event

    // Sub-command bytes read from byte1 of an UpdateRuntime/Object::Trace
    // message (category 0x14, command 0x40). These come from SysexApi.cpp's
    // processDebug() switch statement, not from ElectraCommand.h - there's no
    // enum for them in the firmware header, they're just literal case labels.
    namespace Trace
    {
        constexpr uint8_t EnableDebug = 1;
        constexpr uint8_t SendParameterMap = 10;
        constexpr uint8_t SetBreakpoints = 91;
    } // namespace Trace

} // namespace ElectraCommand
