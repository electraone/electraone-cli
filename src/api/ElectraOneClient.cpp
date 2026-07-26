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

#include "electraone/ElectraOneClient.hpp"

#include <ArduinoJson.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "electra_command.hpp"
#include "md5.hpp"
#include "midi_transport.hpp"

namespace electraone
{
    namespace
    {
        std::vector<uint8_t> optionalBankSlot(std::optional<int> bank,
                                              std::optional<int> slot)
        {
            if (!bank.has_value() && !slot.has_value())
                return {};
            return { static_cast<uint8_t>(bank.value_or(0)),
                     static_cast<uint8_t>(slot.value_or(0)) };
        }

        std::vector<uint8_t> bankSlot(int bank, int slot)
        {
            return { static_cast<uint8_t>(bank), static_cast<uint8_t>(slot) };
        }

        std::vector<uint8_t> jsonToBytes(const JsonDocument &doc)
        {
            std::string out;
            serializeJson(doc, out);
            return sysex::encodeAscii(out);
        }

        std::vector<uint8_t>
            projectSlotJson(const std::string &projectId, int bank, int slot)
        {
            JsonDocument doc;
            doc["projectId"] = projectId;
            doc["bankNumber"] = bank;
            doc["slot"] = slot;
            return jsonToBytes(doc);
        }

    } // namespace

    EventFlags EventFlags::all()
    {
        return { true, true, true, true, true, true, true };
    }

    std::string describeEvent(const Response &r)
    {
        std::ostringstream os;

        if (r.isAck) {
            os << "ACK";
            if (r.transactionId.has_value())
                os << " (txn " << *r.transactionId << ")";
            return os.str();
        }

        if (r.isNack) {
            os << "NACK";
            if (r.transactionId.has_value())
                os << " (txn " << *r.transactionId << ")";
            return os.str();
        }

        auto byteAt = [&](size_t i) -> int {
            return i < r.payload.size() ? r.payload[i] : -1;
        };

        if (r.category == ElectraCommand::Type::Event) {
            switch (r.command) {
                case ElectraCommand::Event::PresetSwitch:
                    os << "Preset Switch: bank=" << byteAt(0)
                       << " slot=" << byteAt(1);
                    return os.str();
                case ElectraCommand::Event::SnapshotChange:
                    os << "Snapshot List Change";
                    return os.str();
                // Not in ElectraCommand::Event (no 0x31 entry there at all) -
                // byte value unverified against the firmware source.
                case 0x31:
                    os << "Capture List Change";
                    return os.str();
                case ElectraCommand::Event::PotTouch:
                    os << "Pot Touch: pot=" << byteAt(0) << " control="
                       << sysex::decode14bit(static_cast<uint8_t>(byteAt(1)),
                                             static_cast<uint8_t>(byteAt(2)))
                       << " touched=" << byteAt(3);
                    return os.str();
                case ElectraCommand::Event::PresetListChange:
                    os << "Preset List Change";
                    return os.str();
                case ElectraCommand::Event::PageSwitch:
                    os << "Page Switch: page=" << byteAt(0);
                    return os.str();
                case ElectraCommand::Event::ControlSetSwitch:
                    os << "Control Set Switch: set=" << byteAt(0);
                    return os.str();
                case ElectraCommand::Event::PresetBankSwitch:
                    os << "Preset Bank Switch: bank=" << byteAt(0);
                    return os.str();
                case ElectraCommand::Event::UsbHostChange:
                    os << "USB Host Change";
                    return os.str();
                case ElectraCommand::Event::SnapshotBankSwitch:
                    os << "Snapshot Bank Switch: bank=" << byteAt(0);
                    return os.str();
                // Not in ElectraCommand::Event (no 0x2D entry there at all) -
                // byte value unverified against the firmware source.
                case 0x2D: {
                    uint32_t bytes = r.payload.size() >= 4
                                         ? sysex::decode28bit(r.payload[0],
                                                              r.payload[1],
                                                              r.payload[2],
                                                              r.payload[3])
                                         : 0;
                    os << "Progress Report: " << bytes << " bytes transferred";
                    return os.str();
                }
                default:
                    break;
            }
        }

        // Command byte 0x00 here isn't in ElectraCommand::Object/Event either -
        // unverified against the firmware source, kept as a literal.
        if (r.category == ElectraCommand::Type::SystemCall
            && r.command == 0x00) {
            os << "Log: " << r.payloadAsText();
            return os.str();
        }

        if (r.category == ElectraCommand::Type::MidiLearnSwitch) {
            os << "MIDI Learn Info: " << static_cast<char>(r.command)
               << r.payloadAsText();
            return os.str();
        }

        os << "0x" << std::hex << static_cast<int>(r.category) << "/0x"
           << static_cast<int>(r.command) << std::dec
           << " payload=" << sysex::toHex(r.payload);
        return os.str();
    }

    struct Client::Impl {
        MidiTransport transport;
        ConnectOptions options;
        bool connected = false;

        std::optional<Response> send(uint8_t category,
                                     uint8_t command,
                                     const std::vector<uint8_t> &params,
                                     int timeoutMs)
        {
            if (!connected)
                throw std::runtime_error(
                    "electraone::Client is not connected - call connect() "
                    "first");
            auto msg = sysex::buildMessage(
                category, command, params, options.transactionId);
            transport.send(msg);

            // Many commands trigger an unsolicited notification event alongside
            // their real reply (e.g. Upload Preset also emits a Preset List
            // Change event) - the device doesn't guarantee which arrives first.
            // Skip past anything that isn't actually our reply (see
            // sysex::isReplyTo) so it can't be mistaken for it. (waitAndParse()
            // itself stays a plain "wait for whatever's next" - that's what
            // poll() needs it to be.)
            while (true) {
                auto resp = waitAndParse(timeoutMs);
                if (!resp.has_value())
                    return std::nullopt;
                if (sysex::isReplyTo(*resp, category, command)) {
                    return resp;
                }
            }
        }

        std::optional<Response> waitAndParse(int timeoutMs)
        {
            int effectiveTimeout =
                timeoutMs >= 0 ? timeoutMs : options.timeoutMs;
            auto raw = transport.waitForReply(effectiveTimeout);
            if (!raw.has_value())
                return std::nullopt;
            return sysex::parseResponse(*raw);
        }
    };

    Client::Client() : impl_(std::make_unique<Impl>())
    {
    }
    Client::~Client() = default;

    std::vector<std::string> Client::listOutputPorts()
    {
        return MidiTransport::listOutputPortNames();
    }

    std::vector<std::string> Client::listInputPorts()
    {
        return MidiTransport::listInputPortNames();
    }

    void Client::connect(const ConnectOptions &options)
    {
        impl_->options = options;
        auto outIndex =
            options.outPortIndex ? options.outPortIndex : options.portIndex;
        auto inIndex =
            options.inPortIndex ? options.inPortIndex : options.portIndex;
        impl_->transport.open(options.portNameSubstring, outIndex, inIndex);
        impl_->connected = true;
    }

    bool Client::isConnected() const
    {
        return impl_->connected;
    }

    std::optional<Response> Client::send(uint8_t category,
                                         uint8_t command,
                                         const std::vector<uint8_t> &params,
                                         int timeoutMs)
    {
        return impl_->send(category, command, params, timeoutMs);
    }

    std::optional<Response>
        Client::sendRawBytes(const std::vector<uint8_t> &fullMessage,
                             int timeoutMs)
    {
        if (!impl_->connected)
            throw std::runtime_error(
                "electraone::Client is not connected - call connect() first");
        impl_->transport.send(fullMessage);
        return impl_->waitAndParse(timeoutMs);
    }

    std::optional<Response> Client::poll(int timeoutMs)
    {
        if (!impl_->connected)
            throw std::runtime_error(
                "electraone::Client is not connected - call connect() first");
        return impl_->waitAndParse(timeoutMs);
    }

    // ---- Info ----
    std::optional<Response> Client::getElectraInfo()
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::ElectraInfo,
                    {});
    }

    std::optional<Response> Client::getRuntimeInfo()
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::RuntimeInfo,
                    {});
    }

    std::optional<Response> Client::reboot()
    {
        return send(ElectraCommand::Type::SystemCall,
                    ElectraCommand::Object::Reboot,
                    {});
    }

    std::optional<Response> Client::setDebug(bool enabled)
    {
        // 0x7C as a category doesn't correspond to any ElectraCommand::Type
        // value (translateType would map it to Type::Unknown) - it numerically
        // matches Object::AppInfo instead. See README.md: this command may not
        // be recognized by current firmware.
        return send(ElectraCommand::Object::AppInfo, enabled ? 0x01 : 0x00, {});
    }

    std::optional<Response> Client::setMidiLearn(bool enabled)
    {
        return send(ElectraCommand::Type::MidiLearnSwitch,
                    enabled ? ElectraCommand::Object::MidiLearnOn
                            : ElectraCommand::Object::MidiLearnOff,
                    {});
    }

    std::optional<Response> Client::getUsbHostDevices()
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::UsbHostList,
                    {});
    }

    std::optional<Response> Client::getParameterMap()
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::ParameterMap,
                    {});
    }

    std::optional<Response> Client::saveScreenshot()
    {
        return send(ElectraCommand::Type::SystemCall,
                    ElectraCommand::Object::Screenshot,
                    {});
    }

    std::optional<Response>
        Client::setBreakpoints(const std::vector<uint16_t> &breakpoints)
    {
        if (breakpoints.size() > 8) {
            throw std::runtime_error(
                "setBreakpoints: at most 8 breakpoints are supported");
        }

        JsonDocument doc;
        auto arr = doc.to<JsonArray>();
        for (uint16_t bp : breakpoints)
            arr.add(bp);

        std::vector<uint8_t> params{ ElectraCommand::Trace::SetBreakpoints };
        auto jsonBytes = jsonToBytes(doc);
        params.insert(params.end(), jsonBytes.begin(), jsonBytes.end());
        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::Trace,
                    params);
    }

    // ---- Preset ----
    std::optional<Response> Client::getPreset(std::optional<int> bank,
                                              std::optional<int> slot)
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::FilePreset,
                    optionalBankSlot(bank, slot));
    }

    std::optional<Response> Client::uploadPreset(const std::string &presetJson)
    {
        return send(ElectraCommand::Type::FileUpload,
                    ElectraCommand::Object::FilePreset,
                    sysex::encodeAscii(presetJson));
    }

    std::optional<Response> Client::removePreset(int bank, int slot)
    {
        return send(ElectraCommand::Type::Remove,
                    ElectraCommand::Object::FilePreset,
                    bankSlot(bank, slot));
    }

    std::optional<Response> Client::clearPresetSlot(int bank, int slot)
    {
        return send(ElectraCommand::Type::Remove,
                    ElectraCommand::Object::PresetSlot,
                    bankSlot(bank, slot));
    }

    std::optional<Response> Client::getPresetList()
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::PresetList,
                    {});
    }

    std::optional<Response> Client::getPresetSlotInfo(int bank, int slot)
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::PresetSlot,
                    bankSlot(bank, slot));
    }

    std::optional<Response> Client::switchPresetSlot(int bank, int slot)
    {
        return send(ElectraCommand::Type::Switch,
                    ElectraCommand::Object::PresetSlot,
                    bankSlot(bank, slot));
    }

    std::optional<Response> Client::setPresetSlot(int bank, int slot)
    {
        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::PresetSlot,
                    bankSlot(bank, slot));
    }

    std::optional<Response> Client::reloadPresetSlot()
    {
        return send(ElectraCommand::Type::Execute,
                    ElectraCommand::Object::PresetSlot,
                    {});
    }

    std::optional<Response>
        Client::loadPreloadedPreset(int bank, int slot, const std::string &path)
    {
        JsonDocument doc;
        doc["bankNumber"] = bank;
        doc["slot"] = slot;
        doc["preset"] = path;
        return send(ElectraCommand::Type::Update,
                    ElectraCommand::Object::PresetSlot,
                    jsonToBytes(doc));
    }

    // ---- Lua ----
    std::optional<Response> Client::getLuaScript(std::optional<int> bank,
                                                 std::optional<int> slot)
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::FileLua,
                    optionalBankSlot(bank, slot));
    }

    std::optional<Response> Client::uploadLuaScript(const std::string &code)
    {
        return send(ElectraCommand::Type::FileUpload,
                    ElectraCommand::Object::FileLua,
                    sysex::encodeAscii(code));
    }

    std::optional<Response> Client::removeLuaScript(int bank, int slot)
    {
        return send(ElectraCommand::Type::Remove,
                    ElectraCommand::Object::FileLua,
                    bankSlot(bank, slot));
    }

    std::optional<Response> Client::executeLuaCommand(const std::string &code)
    {
        return send(ElectraCommand::Type::Execute,
                    ElectraCommand::Object::Function,
                    sysex::encodeAscii(code));
    }

    // ---- Overrides / Persisted / Performance / Config ----
    std::optional<Response> Client::getDeviceOverrides(std::optional<int> bank,
                                                       std::optional<int> slot)
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::DeviceList,
                    optionalBankSlot(bank, slot));
    }

    std::optional<Response>
        Client::uploadDeviceOverrides(const std::string &json)
    {
        return send(ElectraCommand::Type::FileUpload,
                    ElectraCommand::Object::DeviceList,
                    sysex::encodeAscii(json));
    }

    std::optional<Response> Client::getPersistedData(std::optional<int> bank,
                                                     std::optional<int> slot)
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::Datafile,
                    optionalBankSlot(bank, slot));
    }

    std::optional<Response> Client::uploadPersistedData(const std::string &json)
    {
        return send(ElectraCommand::Type::FileUpload,
                    ElectraCommand::Object::Datafile,
                    sysex::encodeAscii(json));
    }

    std::optional<Response> Client::getPerformance(int bank, int slot)
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::Performance,
                    bankSlot(bank, slot));
    }

    std::optional<Response> Client::uploadPerformance(const std::string &json)
    {
        return send(ElectraCommand::Type::FileUpload,
                    ElectraCommand::Object::Performance,
                    sysex::encodeAscii(json));
    }

    std::optional<Response> Client::getConfiguration()
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::FileConfig,
                    {});
    }

    std::optional<Response> Client::uploadConfiguration(const std::string &json)
    {
        return send(ElectraCommand::Type::FileUpload,
                    ElectraCommand::Object::FileConfig,
                    sysex::encodeAscii(json));
    }

    std::optional<Response> Client::removeConfig()
    {
        return send(ElectraCommand::Type::Remove,
                    ElectraCommand::Object::FileConfig,
                    {});
    }

    // ---- Snapshot ----
    std::optional<Response>
        Client::getSnapshotsList(const std::string &projectId)
    {
        JsonDocument doc;
        doc["projectId"] = projectId;
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::SnapshotList,
                    jsonToBytes(doc));
    }

    std::optional<Response>
        Client::getSnapshotData(const std::string &projectId,
                                int bank,
                                int slot)
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::FileSnapshot,
                    projectSlotJson(projectId, bank, slot));
    }

    std::optional<Response> Client::updateSnapshot(const std::string &projectId,
                                                   int bank,
                                                   int slot,
                                                   const std::string &name,
                                                   const std::string &color)
    {
        JsonDocument doc;
        doc["projectId"] = projectId;
        doc["bankNumber"] = bank;
        doc["slot"] = slot;
        doc["name"] = name;
        doc["color"] = color;
        return send(ElectraCommand::Type::Update,
                    ElectraCommand::Object::SnapshotInfo,
                    jsonToBytes(doc));
    }

    std::optional<Response>
        Client::removeSnapshot(const std::string &projectId, int bank, int slot)
    {
        return send(ElectraCommand::Type::Remove,
                    ElectraCommand::Object::SnapshotInfo,
                    projectSlotJson(projectId, bank, slot));
    }

    std::optional<Response> Client::swapSnapshots(const std::string &projectId,
                                                  int fromBank,
                                                  int fromSlot,
                                                  int toBank,
                                                  int toSlot)
    {
        JsonDocument doc;
        doc["projectId"] = projectId;
        doc["fromBankNumber"] = fromBank;
        doc["fromSlot"] = fromSlot;
        doc["toBankNumber"] = toBank;
        doc["toSlot"] = toSlot;
        return send(ElectraCommand::Type::Swap,
                    ElectraCommand::Object::SnapshotInfo,
                    jsonToBytes(doc));
    }

    std::optional<Response>
        Client::setSnapshotSlot(const std::string &projectId,
                                int bank,
                                int slot)
    {
        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::SnapshotSlot,
                    projectSlotJson(projectId, bank, slot));
    }

    // ---- Capture ----
    std::optional<Response>
        Client::getCapturesList(const std::string &projectId)
    {
        JsonDocument doc;
        doc["projectId"] = projectId;
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::CaptureList,
                    jsonToBytes(doc));
    }

    std::optional<Response>
        Client::getCaptureData(const std::string &projectId, int bank, int slot)
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::FileCapture,
                    projectSlotJson(projectId, bank, slot));
    }

    std::optional<Response> Client::updateCapture(const std::string &projectId,
                                                  int bank,
                                                  int slot,
                                                  const std::string &name,
                                                  const std::string &color)
    {
        JsonDocument doc;
        doc["projectId"] = projectId;
        doc["bankNumber"] = bank;
        doc["slot"] = slot;
        doc["name"] = name;
        doc["color"] = color;
        return send(ElectraCommand::Type::Update,
                    ElectraCommand::Object::CaptureInfo,
                    jsonToBytes(doc));
    }

    std::optional<Response>
        Client::removeCapture(const std::string &projectId, int bank, int slot)
    {
        return send(ElectraCommand::Type::Remove,
                    ElectraCommand::Object::CaptureInfo,
                    projectSlotJson(projectId, bank, slot));
    }

    std::optional<Response> Client::swapCaptures(const std::string &projectId,
                                                 int fromBank,
                                                 int fromSlot,
                                                 int toBank,
                                                 int toSlot)
    {
        JsonDocument doc;
        doc["projectId"] = projectId;
        doc["fromBankNumber"] = fromBank;
        doc["fromSlot"] = fromSlot;
        doc["toBankNumber"] = toBank;
        doc["toSlot"] = toSlot;
        return send(ElectraCommand::Type::Swap,
                    ElectraCommand::Object::CaptureInfo,
                    jsonToBytes(doc));
    }

    std::optional<Response>
        Client::setCaptureSlot(const std::string &projectId, int bank, int slot)
    {
        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::CaptureSlot,
                    projectSlotJson(projectId, bank, slot));
    }

    // ---- Control ----
    std::optional<Response> Client::updateControl(int id,
                                                  const ControlUpdate &update)
    {
        auto idBytes = sysex::encode14bit(static_cast<uint16_t>(id));
        std::vector<uint8_t> params{ idBytes[0], idBytes[1] };

        JsonDocument doc;
        if (update.name.has_value())
            doc["name"] = *update.name;
        if (update.color.has_value())
            doc["color"] = *update.color;
        if (update.visible.has_value())
            doc["visible"] = *update.visible;
        if (update.valueText.has_value()) {
            auto valueObj = doc["value"].to<JsonObject>();
            if (update.valueId.has_value())
                valueObj["id"] = *update.valueId;
            valueObj["text"] = *update.valueText;
        }
        auto jsonBytes = jsonToBytes(doc);
        params.insert(params.end(), jsonBytes.begin(), jsonBytes.end());

        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::Control,
                    params);
    }

    std::optional<Response>
        Client::overrideValueText(int id, int valueId, const std::string &text)
    {
        if (text.size() > 15)
            throw std::runtime_error(
                "override text must be at most 15 characters");
        auto idBytes = sysex::encode14bit(static_cast<uint16_t>(id));
        std::vector<uint8_t> params{ idBytes[0],
                                     idBytes[1],
                                     static_cast<uint8_t>(valueId) };
        auto textBytes = sysex::encodeAscii(text);
        params.insert(params.end(), textBytes.begin(), textBytes.end());
        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::Value,
                    params);
    }

    // ---- UI ----
    std::optional<Response> Client::switchPage(int page)
    {
        return send(ElectraCommand::Type::Switch,
                    ElectraCommand::Object::Page,
                    { static_cast<uint8_t>(page) });
    }

    std::optional<Response> Client::switchControlSet(int set)
    {
        return send(ElectraCommand::Type::Switch,
                    ElectraCommand::Object::ControlSet,
                    { static_cast<uint8_t>(set) });
    }

    std::optional<Response> Client::setBottomBarText(const std::string &text)
    {
        if (text.size() > 40)
            throw std::runtime_error(
                "bottom bar text must be at most 40 characters");
        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::StatusBar,
                    sysex::encodeAscii(text));
    }

    // ---- Events / Logger / Window ----
    std::optional<Response> Client::setEventsMidiPort(Port port)
    {
        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::ControlPort,
                    { static_cast<uint8_t>(port) });
    }

    std::optional<Response> Client::subscribeEvents(const EventFlags &flags)
    {
        uint8_t byte = 0;
        if (flags.page)
            byte |= 1 << 0;
        if (flags.controlSet)
            byte |= 1 << 1;
        if (flags.usbHost)
            byte |= 1 << 2;
        if (flags.pots)
            byte |= 1 << 3;
        if (flags.touch)
            byte |= 1 << 4;
        if (flags.button)
            byte |= 1 << 5;
        if (flags.window)
            byte |= 1 << 6;
        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::EventSubscription,
                    { byte });
    }

    std::optional<Response> Client::enableLogger()
    {
        return send(ElectraCommand::Type::SystemCall,
                    ElectraCommand::Object::Logger,
                    { 0x01, 0x00 });
    }

    std::optional<Response> Client::disableLogger()
    {
        return send(ElectraCommand::Type::SystemCall,
                    ElectraCommand::Object::Logger,
                    { 0x00, 0x00 });
    }

    std::optional<Response> Client::setLoggerMidiPort(Port port)
    {
        return send(ElectraCommand::Type::UpdateRuntime,
                    ElectraCommand::Object::Logger,
                    { static_cast<uint8_t>(port), 0x00 });
    }

    std::optional<Response> Client::stopWindowRepaints()
    {
        return send(ElectraCommand::Type::SystemCall,
                    ElectraCommand::Object::Window,
                    { 0x00, 0x00 });
    }

    std::optional<Response> Client::resumeWindowRepaints()
    {
        return send(ElectraCommand::Type::SystemCall,
                    ElectraCommand::Object::Window,
                    { 0x01, 0x00 });
    }

    // ---- File Transfer ----
    std::optional<Response> Client::openCacheTransaction()
    {
        return send(ElectraCommand::Type::FileUpload,
                    ElectraCommand::Object::FileStagedCache,
                    {});
    }

    std::optional<Response> Client::registerFile(int id, uint32_t size)
    {
        auto sizeBytes = sysex::encode28bit(size);
        std::vector<uint8_t> params{ static_cast<uint8_t>(id) };
        params.insert(params.end(), sizeBytes.begin(), sizeBytes.end());
        return send(ElectraCommand::Type::FileUpload,
                    ElectraCommand::Object::FileStagedHeader,
                    params);
    }

    std::optional<Response> Client::sendChunk(
        int id,
        const std::vector<uint8_t> &data,
        bool allowBinary,
        const std::function<void(const Response &)> &onProgress)
    {
        std::vector<uint8_t> bytes = data;
        if (!allowBinary)
            bytes = sysex::encodeAscii(std::string(data.begin(), data.end()));

        std::vector<uint8_t> params{ static_cast<uint8_t>(id) };
        params.insert(params.end(), bytes.begin(), bytes.end());

        if (!impl_->connected)
            throw std::runtime_error(
                "electraone::Client is not connected - call connect() first");
        auto msg = sysex::buildMessage(ElectraCommand::Type::FileUpload,
                                       ElectraCommand::Object::FileStagedChunk,
                                       params,
                                       impl_->options.transactionId);
        impl_->transport.send(msg);

        while (true) {
            auto resp = impl_->waitAndParse(-1);
            if (!resp.has_value())
                return std::nullopt; // timeout
            if (resp->isAck || resp->isNack)
                return resp;
            if (onProgress)
                onProgress(*resp);
        }
    }

    std::optional<Response>
        Client::commitTransaction(const std::string &commitJson, int timeoutMs)
    {
        return send(ElectraCommand::Type::Update,
                    ElectraCommand::Object::FileStagedCache,
                    sysex::encodeAscii(commitJson),
                    timeoutMs);
    }

    std::optional<Response>
        Client::getLocationFiles(const std::string &locationJson)
    {
        return send(ElectraCommand::Type::FileRequest,
                    ElectraCommand::Object::Location,
                    sysex::encodeAscii(locationJson));
    }

    std::optional<Response>
        Client::removeLocationFiles(const std::string &locationJson)
    {
        return send(ElectraCommand::Type::Remove,
                    ElectraCommand::Object::Location,
                    sysex::encodeAscii(locationJson));
    }

    Response Client::uploadFile(const std::vector<uint8_t> &content,
                                const UploadFileOptions &options)
    {
        if (options.chunkSize <= 0)
            throw std::runtime_error(
                "UploadFileOptions::chunkSize must be positive");

        if (!options.allowBinary) {
            for (unsigned char c : content) {
                if (c >= 0x80) {
                    throw std::runtime_error("file contains a byte >= 0x80");
                }
            }
        }

        auto require = [](const std::optional<Response> &resp,
                          const char *step) -> Response {
            if (!resp.has_value())
                throw std::runtime_error(std::string("timed out during ")
                                         + step);
            if (resp->isNack)
                throw std::runtime_error(std::string(step) + " was NACKed");
            return *resp;
        };

        require(openCacheTransaction(), "open cache transaction");
        require(registerFile(options.id, static_cast<uint32_t>(content.size())),
                "register file");

        size_t sent = 0;
        while (sent < content.size()) {
            size_t len = std::min(static_cast<size_t>(options.chunkSize),
                                  content.size() - sent);
            std::vector<uint8_t> chunk(
                content.begin() + static_cast<long>(sent),
                content.begin() + static_cast<long>(sent + len));
            auto resp =
                sendChunk(options.id, chunk, options.allowBinary, nullptr);
            require(resp, "send chunk");
            sent += len;
            if (options.onProgress)
                options.onProgress(sent, content.size());
        }

        std::string md5 = md5Hex(content);
        JsonDocument doc;
        auto filesArray = doc["files"].to<JsonArray>();
        auto fileObj = filesArray.add<JsonObject>();
        fileObj["id"] = options.id;
        fileObj["location"] = options.location;
        fileObj["type"] = options.type;
        if (options.bank.has_value())
            fileObj["bankNumber"] = *options.bank;
        if (options.slot.has_value())
            fileObj["slot"] = *options.slot;
        if (options.ns.has_value())
            fileObj["namespace"] = *options.ns;
        if (options.path.has_value())
            fileObj["path"] = *options.path;
        fileObj["md5"] = md5;
        std::string commitJson;
        serializeJson(doc, commitJson);

        return require(commitTransaction(commitJson, options.commitTimeoutMs),
                       "commit transaction");
    }
} // namespace electraone