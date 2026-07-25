#include "electraone/ElectraOneClient.hpp"

#include <ArduinoJson.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>

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

        if (r.category == 0x7E) {
            switch (r.command) {
                case 0x02:
                    os << "Preset Switch: bank=" << byteAt(0)
                       << " slot=" << byteAt(1);
                    return os.str();
                case 0x03:
                    os << "Snapshot List Change";
                    return os.str();
                case 0x31:
                    os << "Capture List Change";
                    return os.str();
                case 0x0A:
                    os << "Pot Touch: pot=" << byteAt(0) << " control="
                       << sysex::decode14bit(static_cast<uint8_t>(byteAt(1)),
                                             static_cast<uint8_t>(byteAt(2)))
                       << " touched=" << byteAt(3);
                    return os.str();
                case 0x05:
                    os << "Preset List Change";
                    return os.str();
                case 0x06:
                    os << "Page Switch: page=" << byteAt(0);
                    return os.str();
                case 0x07:
                    os << "Control Set Switch: set=" << byteAt(0);
                    return os.str();
                case 0x08:
                    if (r.payload.empty()) {
                        os << "USB Host Change";
                    } else {
                        os << "Preset Bank Switch: bank=" << byteAt(0);
                    }
                    return os.str();
                case 0x04:
                    os << "Snapshot Bank Switch: bank=" << byteAt(0);
                    return os.str();
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
        if (r.category == 0x7F && r.command == 0x00) {
            os << "Log: " << r.payloadAsText();
            return os.str();
        }
        if (r.category == 0x03) {
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
                    "electraone::Client is not connected - call connect() first");
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
        return send(0x02, 0x7F, {});
    }
    std::optional<Response> Client::getRuntimeInfo()
    {
        return send(0x02, 0x7E, {});
    }
    std::optional<Response> Client::reboot()
    {
        return send(0x7F, 0x78, {});
    }
    std::optional<Response> Client::setDebug(bool enabled)
    {
        return send(0x7C, enabled ? 0x01 : 0x00, {});
    }
    std::optional<Response> Client::setMidiLearn(bool enabled)
    {
        return send(0x03, enabled ? 0x01 : 0x00, {});
    }
    std::optional<Response> Client::getUsbHostDevices()
    {
        return send(0x02, 0x10, {});
    }

    // ---- Preset ----
    std::optional<Response> Client::getPreset(std::optional<int> bank,
                                              std::optional<int> slot)
    {
        return send(0x02, 0x01, optionalBankSlot(bank, slot));
    }
    std::optional<Response> Client::uploadPreset(const std::string &presetJson)
    {
        return send(0x01, 0x01, sysex::encodeAscii(presetJson));
    }
    std::optional<Response> Client::removePreset(int bank, int slot)
    {
        return send(0x05, 0x01, bankSlot(bank, slot));
    }
    std::optional<Response> Client::clearPresetSlot(int bank, int slot)
    {
        return send(0x05, 0x08, bankSlot(bank, slot));
    }
    std::optional<Response> Client::getPresetList()
    {
        return send(0x02, 0x04, {});
    }
    std::optional<Response> Client::getPresetSlotInfo(int bank, int slot)
    {
        return send(0x02, 0x08, bankSlot(bank, slot));
    }
    std::optional<Response> Client::switchPresetSlot(int bank, int slot)
    {
        return send(0x09, 0x08, bankSlot(bank, slot));
    }
    std::optional<Response> Client::setPresetSlot(int bank, int slot)
    {
        return send(0x14, 0x08, bankSlot(bank, slot));
    }
    std::optional<Response> Client::reloadPresetSlot()
    {
        return send(0x08, 0x08, {});
    }
    std::optional<Response>
        Client::loadPreloadedPreset(int bank, int slot, const std::string &path)
    {
        JsonDocument doc;
        doc["bankNumber"] = bank;
        doc["slot"] = slot;
        doc["path"] = path;
        return send(0x04, 0x08, jsonToBytes(doc));
    }

    // ---- Lua ----
    std::optional<Response> Client::getLuaScript(std::optional<int> bank,
                                                 std::optional<int> slot)
    {
        return send(0x02, 0x0C, optionalBankSlot(bank, slot));
    }
    std::optional<Response> Client::uploadLuaScript(const std::string &code)
    {
        return send(0x01, 0x0C, sysex::encodeAscii(code));
    }
    std::optional<Response> Client::removeLuaScript(int bank, int slot)
    {
        return send(0x05, 0x0C, bankSlot(bank, slot));
    }
    std::optional<Response> Client::executeLuaCommand(const std::string &code)
    {
        return send(0x08, 0x0D, sysex::encodeAscii(code));
    }

    // ---- Overrides / Persisted / Performance / Config ----
    std::optional<Response> Client::getDeviceOverrides(std::optional<int> bank,
                                                       std::optional<int> slot)
    {
        return send(0x02, 0x0F, optionalBankSlot(bank, slot));
    }
    std::optional<Response>
        Client::uploadDeviceOverrides(const std::string &json)
    {
        return send(0x01, 0x0F, sysex::encodeAscii(json));
    }
    std::optional<Response> Client::getPersistedData(std::optional<int> bank,
                                                     std::optional<int> slot)
    {
        return send(0x02, 0x12, optionalBankSlot(bank, slot));
    }
    std::optional<Response> Client::uploadPersistedData(const std::string &json)
    {
        return send(0x01, 0x12, sysex::encodeAscii(json));
    }
    std::optional<Response> Client::getPerformance(int bank, int slot)
    {
        return send(0x02, 0x11, bankSlot(bank, slot));
    }
    std::optional<Response> Client::uploadPerformance(const std::string &json)
    {
        return send(0x01, 0x11, sysex::encodeAscii(json));
    }
    std::optional<Response> Client::getConfiguration()
    {
        return send(0x02, 0x02, {});
    }
    std::optional<Response> Client::uploadConfiguration(const std::string &json)
    {
        return send(0x01, 0x02, sysex::encodeAscii(json));
    }
    std::optional<Response> Client::removeConfig()
    {
        return send(0x05, 0x02, {});
    }

    // ---- Snapshot ----
    std::optional<Response>
        Client::getSnapshotsList(const std::string &projectId)
    {
        JsonDocument doc;
        doc["projectId"] = projectId;
        return send(0x02, 0x05, jsonToBytes(doc));
    }
    std::optional<Response>
        Client::getSnapshotData(const std::string &projectId,
                                int bank,
                                int slot)
    {
        return send(0x02, 0x03, projectSlotJson(projectId, bank, slot));
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
        return send(0x04, 0x06, jsonToBytes(doc));
    }
    std::optional<Response>
        Client::removeSnapshot(const std::string &projectId, int bank, int slot)
    {
        return send(0x05, 0x06, projectSlotJson(projectId, bank, slot));
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
        return send(0x06, 0x06, jsonToBytes(doc));
    }
    std::optional<Response>
        Client::setSnapshotSlot(const std::string &projectId,
                                int bank,
                                int slot)
    {
        return send(0x14, 0x09, projectSlotJson(projectId, bank, slot));
    }

    // ---- Capture ----
    std::optional<Response>
        Client::getCapturesList(const std::string &projectId)
    {
        JsonDocument doc;
        doc["projectId"] = projectId;
        return send(0x02, 0x31, jsonToBytes(doc));
    }
    std::optional<Response>
        Client::getCaptureData(const std::string &projectId, int bank, int slot)
    {
        return send(0x02, 0x30, projectSlotJson(projectId, bank, slot));
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
        return send(0x04, 0x06, jsonToBytes(doc));
    }
    std::optional<Response>
        Client::removeCapture(const std::string &projectId, int bank, int slot)
    {
        return send(0x05, 0x06, projectSlotJson(projectId, bank, slot));
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
        return send(0x06, 0x06, jsonToBytes(doc));
    }
    std::optional<Response>
        Client::setCaptureSlot(const std::string &projectId, int bank, int slot)
    {
        return send(0x14, 0x33, projectSlotJson(projectId, bank, slot));
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
        if (update.value.has_value())
            doc["value"] = *update.value;
        auto jsonBytes = jsonToBytes(doc);
        params.insert(params.end(), jsonBytes.begin(), jsonBytes.end());

        return send(0x14, 0x07, params);
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
        return send(0x14, 0x0E, params);
    }

    // ---- UI ----
    std::optional<Response> Client::switchPage(int page)
    {
        return send(0x09, 0x0A, { static_cast<uint8_t>(page) });
    }
    std::optional<Response> Client::switchControlSet(int set)
    {
        return send(0x09, 0x0B, { static_cast<uint8_t>(set) });
    }
    std::optional<Response> Client::setBottomBarText(const std::string &text)
    {
        if (text.size() > 40)
            throw std::runtime_error(
                "bottom bar text must be at most 40 characters");
        return send(0x14, 0x77, sysex::encodeAscii(text));
    }

    // ---- Events / Logger / Window ----
    std::optional<Response> Client::setEventsMidiPort(Port port)
    {
        return send(0x14, 0x7B, { static_cast<uint8_t>(port) });
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
        return send(0x14, 0x79, { byte });
    }
    std::optional<Response> Client::enableLogger()
    {
        return send(0x7F, 0x7D, { 0x01, 0x00 });
    }
    std::optional<Response> Client::disableLogger()
    {
        return send(0x7F, 0x7D, { 0x00, 0x00 });
    }
    std::optional<Response> Client::setLoggerMidiPort(Port port)
    {
        return send(0x14, 0x7D, { static_cast<uint8_t>(port), 0x00 });
    }
    std::optional<Response> Client::stopWindowRepaints()
    {
        return send(0x7F, 0x7A, { 0x00, 0x00 });
    }
    std::optional<Response> Client::resumeWindowRepaints()
    {
        return send(0x7F, 0x7A, { 0x01, 0x00 });
    }

    // ---- File Transfer ----
    std::optional<Response> Client::openCacheTransaction()
    {
        return send(0x01, 0x2D, {});
    }
    std::optional<Response> Client::registerFile(int id, uint32_t size)
    {
        auto sizeBytes = sysex::encode28bit(size);
        std::vector<uint8_t> params{ static_cast<uint8_t>(id) };
        params.insert(params.end(), sizeBytes.begin(), sizeBytes.end());
        return send(0x01, 0x2E, params);
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
        auto msg = sysex::buildMessage(
            0x01, 0x2F, params, impl_->options.transactionId);
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
        return send(0x04, 0x2D, sysex::encodeAscii(commitJson), timeoutMs);
    }
    std::optional<Response>
        Client::getLocationFiles(const std::string &locationJson)
    {
        return send(0x02, 0x34, sysex::encodeAscii(locationJson));
    }
    std::optional<Response>
        Client::removeLocationFiles(const std::string &locationJson)
    {
        return send(0x05, 0x34, sysex::encodeAscii(locationJson));
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
                    throw std::runtime_error(
                        "file contains a byte >= 0x80; the chunk encoding for real binary data isn't verified "
                        "(see README.md) - set UploadFileOptions::allowBinary to attempt it anyway");
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
