#pragma once

// Public C++ API for the Electra One SysEx protocol (core implementation +
// File Transfer API), built on RtMidi. This is the library form of the
// `electraone` CLI in this repository - link against libelectraone.a to
// drive an Electra One's CTRL port directly from a C++ application instead
// of shelling out to the CLI.
//
// Usage sketch:
//
//   electraone::Client client;
//   client.connect();                          // finds the CTRL port by name
//   auto info = client.getElectraInfo();
//   if (info && info->isAck == false && !info->isNack) {
//       std::cout << info->payloadAsText() << "\n";   // JSON text
//   }
//
// Every command method returns std::optional<Response>: std::nullopt means
// the device didn't reply within the timeout; otherwise check
// Response::isNack (command rejected) vs. the payload (query results) or
// Response::isAck (plain success acknowledgement). See sysex.hpp for the
// Response (= sysex::ParsedResponse) definition.
//
// Thread safety: a Client is not safe to use from multiple threads
// concurrently - it holds one open MIDI connection and expects a strict
// request/reply cadence (matching how the Electra One's SysEx protocol
// itself works: one outstanding request at a time).

#include "electraone/sysex.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace electraone {

// A decoded reply, event, or unsolicited message from the device. Alias for
// sysex::ParsedResponse so this header and sysex.hpp share one definition.
using Response = sysex::ParsedResponse;

struct ConnectOptions {
    // Case-insensitive substring to match against MIDI port names; the
    // default finds the Electra One's CTRL port ("Electra Controller
    // Electra CTRL" on most systems) with no further configuration.
    std::string portNameSubstring = "CTRL";
    // If set, use this MIDI port index directly (see Client::listOutputPorts
    // / listInputPorts) instead of matching by name.
    std::optional<unsigned int> portIndex;
    // Default reply timeout for every command method below, in milliseconds.
    int timeoutMs = 3000;
    // Optional 14-bit transaction ID attached to every outgoing request
    // (firmware 4.0+); ACK/NACK replies echo it back in Response::transactionId.
    std::optional<uint16_t> transactionId;
};

enum class Port : uint8_t { Port1 = 0x00, Port2 = 0x01, Ctrl = 0x02 };

// Subscribe Events flag set; see Client::subscribeEvents.
struct EventFlags {
    bool page = false;
    bool controlSet = false;
    bool usbHost = false;
    bool pots = false;
    bool touch = false;
    bool button = false;
    bool window = false;

    static EventFlags all();
};

// Fields to change via Client::updateControl; leave a field unset (nullopt)
// to leave that property alone.
struct ControlUpdate {
    std::optional<std::string> name;
    std::optional<std::string> color;
    std::optional<bool> visible;
    std::optional<int> value;
};

// Destination + options for Client::uploadFile. location/type follow the
// File Transfer API's vocabulary: location is one of "slots", "updates",
// "assets", "modules", "presets", "root"; type is one of "firmware",
// "bootloader", "preset", "lua", "luaModule", "ui", "config", "deviceList",
// "datafile", "performance". Set bank/slot for "slots" destinations, or
// ns/path for "modules"/"presets" destinations, per the destination's needs.
struct UploadFileOptions {
    std::string location;
    std::string type;
    std::optional<int> bank;
    std::optional<int> slot;
    std::optional<std::string> ns;
    std::optional<std::string> path;
    int id = 1;              // only needs to be unique within this transaction
    int chunkSize = 256;     // bytes of file data per Transfer Chunks message
    // The device's Transfer Chunks payload encoding for arbitrary 8-bit bytes
    // isn't documented (see README.md); by default every byte is checked to
    // be < 0x80 and rejected otherwise. Set true to bypass that check for
    // genuinely binary content (firmware/bootloader) - it's still sent
    // unpacked, so it will most likely fail an MD5 mismatch at commit rather
    // than corrupt anything (commit is atomic), but there's no verified
    // packing scheme to fall back to.
    bool allowBinary = false;
    // Called after each chunk is acknowledged, with (bytesSentSoFar, totalBytes).
    std::function<void(size_t, size_t)> onProgress;
};

// One-line human-readable rendering of a Response, e.g. "Pot Touch: pot=3
// control=1042 touched=1" or "ACK" or "NACK". Useful for logging whatever
// comes back from Client::poll(). Mirrors the electraone CLI's `events listen`.
std::string describeEvent(const Response& response);

class Client {
public:
    Client();
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Enumerates MIDI port names, for building your own port picker instead
    // of relying on ConnectOptions::portNameSubstring.
    static std::vector<std::string> listOutputPorts();
    static std::vector<std::string> listInputPorts();

    // Opens the matching MIDI port pair. Throws std::runtime_error (with the
    // candidate port list in what()) if the match isn't exactly one port.
    void connect(const ConnectOptions& options = {});
    bool isConnected() const;

    // ---- Low-level ----

    // Sends an arbitrary category/command/payload and waits for a reply.
    // timeoutMs < 0 uses the ConnectOptions timeout. std::nullopt on timeout.
    std::optional<Response> send(uint8_t category, uint8_t command, const std::vector<uint8_t>& params = {},
                                  int timeoutMs = -1);
    // Sends a fully-formed message (including F0...F7) as-is.
    std::optional<Response> sendRawBytes(const std::vector<uint8_t>& fullMessage, int timeoutMs = -1);
    // Waits for the next incoming message without sending anything first -
    // a reply to something already sent, or an unsolicited event (Pot
    // Touch, Page Switch, Log Message, Report Progress, ...).
    std::optional<Response> poll(int timeoutMs);

    // ---- Info ----
    std::optional<Response> getElectraInfo();
    std::optional<Response> getRuntimeInfo();
    std::optional<Response> reboot();
    std::optional<Response> setDebug(bool enabled);
    std::optional<Response> setMidiLearn(bool enabled);
    std::optional<Response> getUsbHostDevices();

    // ---- Preset ----
    // getPreset/getLuaScript/getDeviceOverrides/getPersistedData: pass no
    // bank/slot (or leave both nullopt) to query the current preset.
    std::optional<Response> getPreset(std::optional<int> bank = std::nullopt, std::optional<int> slot = std::nullopt);
    std::optional<Response> uploadPreset(const std::string& presetJson);
    std::optional<Response> removePreset(int bank, int slot);
    std::optional<Response> clearPresetSlot(int bank, int slot);
    std::optional<Response> getPresetList();
    std::optional<Response> getPresetSlotInfo(int bank, int slot);
    std::optional<Response> switchPresetSlot(int bank, int slot);   // runtime, volatile
    std::optional<Response> setPresetSlot(int bank, int slot);      // runtime, volatile
    std::optional<Response> reloadPresetSlot();
    std::optional<Response> loadPreloadedPreset(int bank, int slot, const std::string& path);  // runtime, volatile

    // ---- Lua ----
    std::optional<Response> getLuaScript(std::optional<int> bank = std::nullopt,
                                          std::optional<int> slot = std::nullopt);
    std::optional<Response> uploadLuaScript(const std::string& code);
    std::optional<Response> removeLuaScript(int bank, int slot);
    std::optional<Response> executeLuaCommand(const std::string& code);  // runtime, max 65535 bytes

    // ---- Overrides / Persisted / Performance / Config ----
    std::optional<Response> getDeviceOverrides(std::optional<int> bank = std::nullopt,
                                                std::optional<int> slot = std::nullopt);
    std::optional<Response> uploadDeviceOverrides(const std::string& json);
    std::optional<Response> getPersistedData(std::optional<int> bank = std::nullopt,
                                              std::optional<int> slot = std::nullopt);
    std::optional<Response> uploadPersistedData(const std::string& json);
    std::optional<Response> getPerformance(int bank, int slot);
    std::optional<Response> uploadPerformance(const std::string& json);
    std::optional<Response> getConfiguration();
    std::optional<Response> uploadConfiguration(const std::string& json);
    std::optional<Response> removeConfig();

    // ---- Snapshot ----
    std::optional<Response> getSnapshotsList(const std::string& projectId);
    std::optional<Response> getSnapshotData(const std::string& projectId, int bank, int slot);
    std::optional<Response> updateSnapshot(const std::string& projectId, int bank, int slot, const std::string& name,
                                            const std::string& color);
    std::optional<Response> removeSnapshot(const std::string& projectId, int bank, int slot);
    std::optional<Response> swapSnapshots(const std::string& projectId, int fromBank, int fromSlot, int toBank,
                                           int toSlot);
    std::optional<Response> setSnapshotSlot(const std::string& projectId, int bank, int slot);  // runtime, volatile

    // ---- Capture ----
    // NOTE: the Electra docs give Update/Remove/Swap Capture the exact same
    // category/command bytes as the Snapshot equivalents, with nothing in
    // the JSON to disambiguate - almost certainly a docs error. These three
    // send the documented (probably wrong) bytes; see README.md. Use
    // Client::send() to override once you've confirmed the real values.
    std::optional<Response> getCapturesList(const std::string& projectId);
    std::optional<Response> getCaptureData(const std::string& projectId, int bank, int slot);
    std::optional<Response> updateCapture(const std::string& projectId, int bank, int slot, const std::string& name,
                                           const std::string& color);
    std::optional<Response> removeCapture(const std::string& projectId, int bank, int slot);
    std::optional<Response> swapCaptures(const std::string& projectId, int fromBank, int fromSlot, int toBank,
                                          int toSlot);
    std::optional<Response> setCaptureSlot(const std::string& projectId, int bank, int slot);  // runtime, volatile

    // ---- Control (runtime) ----
    std::optional<Response> updateControl(int id, const ControlUpdate& update);
    std::optional<Response> overrideValueText(int id, int valueId, const std::string& text);  // max 15 chars

    // ---- UI (runtime) ----
    std::optional<Response> switchPage(int page);
    std::optional<Response> switchControlSet(int set);
    std::optional<Response> setBottomBarText(const std::string& text);  // max 40 chars

    // ---- Events / Logger / Window (runtime) ----
    std::optional<Response> setEventsMidiPort(Port port);
    std::optional<Response> subscribeEvents(const EventFlags& flags);
    std::optional<Response> enableLogger();
    std::optional<Response> disableLogger();
    std::optional<Response> setLoggerMidiPort(Port port);
    std::optional<Response> stopWindowRepaints();
    std::optional<Response> resumeWindowRepaints();

    // ---- File Transfer API (firmware 4.0+) ----
    // Low-level primitives - for the common single-file case, prefer
    // uploadFile() below, which runs all four steps as one transaction.
    std::optional<Response> openCacheTransaction();
    std::optional<Response> registerFile(int id, uint32_t size);
    // Sends one chunk of file data and waits for its ACK/NACK, invoking
    // onProgress for any Report Progress events seen while waiting (the
    // device may interleave these before the chunk's real ACK).
    std::optional<Response> sendChunk(int id, const std::vector<uint8_t>& data, bool allowBinary = false,
                                       const std::function<void(const Response&)>& onProgress = {});
    std::optional<Response> commitTransaction(const std::string& commitJson);
    std::optional<Response> getLocationFiles(const std::string& locationJson);
    std::optional<Response> removeLocationFiles(const std::string& locationJson);

    // Runs open + register + all chunks + commit as one transaction,
    // computing the MD5 the commit step requires. Throws std::runtime_error
    // (naming the failed step) if any step is NACKed or times out.
    Response uploadFile(const std::vector<uint8_t>& content, const UploadFileOptions& options);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace electraone
