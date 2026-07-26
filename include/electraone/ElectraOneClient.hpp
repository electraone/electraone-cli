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
 * @file ElectraOneClient.hpp
 *
 * @brief Public C++ API for the Electra One SysEx protocol (core
 * implementation + File Transfer API), built on RtMidi. This is the library
 * form of the `electraone` CLI in this repository - link against
 * libelectraone.a to drive an Electra One's CTRL port directly from a C++
 * application instead of shelling out to the CLI.
 *
 * Every command method returns std::optional<Response>: std::nullopt means
 * the device didn't reply within the timeout; otherwise check
 * Response::isNack (command rejected) vs. the payload (query results) or
 * Response::isAck (plain success acknowledgement). See sysex.hpp for the
 * Response (= sysex::ParsedResponse) definition.
 *
 * @note Thread safety: a Client is not safe to use from multiple threads
 * concurrently - it holds one open MIDI connection and expects a strict
 * request/reply cadence (matching how the Electra One's SysEx protocol
 * itself works: one outstanding request at a time).
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "electraone/sysex.hpp"

namespace electraone
{

    /// A decoded reply, event, or unsolicited message from the device. Alias
    /// for sysex::ParsedResponse so this header and sysex.hpp share one
    /// definition.
    using Response = sysex::ParsedResponse;

    /// @brief Options for Client::connect.
    struct ConnectOptions {
        /// Case-insensitive substring to match against MIDI port names; the
        /// default finds the Electra One's CTRL port ("Electra Controller
        /// Electra CTRL" on most systems) with no further configuration.
        std::string portNameSubstring = "CTRL";
        /// If set, use this MIDI port index directly for both output and
        /// input (see Client::listOutputPorts / listInputPorts) instead of
        /// matching by name.
        std::optional<unsigned int> portIndex;
        /**
         * @brief Override portIndex for just the output (or input) port.
         *
         * Needed when the output and input port lists are different lengths
         * - e.g. on Windows, a software-only synth can show up as an extra
         * output port with no matching input, shifting every real device's
         * output index out of alignment with its input index - so a single
         * shared portIndex can't address both directions correctly.
         */
        std::optional<unsigned int> outPortIndex;
        std::optional<unsigned int> inPortIndex; ///< @see outPortIndex
        /// Default reply timeout for every command method below, in
        /// milliseconds.
        int timeoutMs = 3000;
        /// Optional 14-bit transaction ID attached to every outgoing request
        /// (firmware 4.0+); ACK/NACK replies echo it back in
        /// Response::transactionId.
        std::optional<uint16_t> transactionId;
    };

    enum class Port : uint8_t { Port1 = 0x00, Port2 = 0x01, Ctrl = 0x02 };

    /// @brief Subscribe Events flag set; see Client::subscribeEvents.
    struct EventFlags {
        bool page = false;
        bool controlSet = false;
        bool usbHost = false;
        bool pots = false;
        bool touch = false;
        bool button = false;
        bool window = false;

        /// @return An EventFlags with every flag set to true.
        static EventFlags all();
    };

    /// @brief Fields to change via Client::updateControl; leave a field
    /// unset (nullopt) to leave that property alone.
    struct ControlUpdate {
        std::optional<std::string> name;
        std::optional<std::string> color;
        std::optional<bool> visible;
        /**
         * @brief Which value handle valueText applies to.
         *
         * Overrides the displayed text for one of the control's value
         * handles - the firmware's SysexApi::updateControl reads this as
         * {"value": {"id": ..., "text": ...}}, not a raw number. Only
         * valueText is required; valueId defaults to the control's main
         * "value" handle (matching the firmware's own default) if left
         * unset.
         */
        std::optional<std::string> valueId;
        std::optional<std::string> valueText; ///< @see valueId
    };

    /**
     * @brief Destination + options for Client::uploadFile.
     *
     * location/type follow the File Transfer API's vocabulary: location is
     * one of "slots", "updates", "assets", "modules", "presets", "root";
     * type is one of "firmware", "bootloader", "preset", "lua", "luaModule",
     * "ui", "config", "deviceList", "datafile", "performance". Set bank/slot
     * for "slots" destinations, or ns/path for "modules"/"presets"
     * destinations, per the destination's needs.
     */
    struct UploadFileOptions {
        std::string location;
        std::string type;
        std::optional<int> bank;
        std::optional<int> slot;
        std::optional<std::string> ns;
        std::optional<std::string> path;
        int id = 1; ///< Only needs to be unique within this transaction.
        int chunkSize =
            4096; ///< Bytes of file data per Transfer Chunks message.
        /**
         * @brief Bypass the 7-bit ASCII check for genuinely binary content.
         *
         * The device's Transfer Chunks payload encoding for arbitrary 8-bit
         * bytes isn't documented (see README.md); by default every byte is
         * checked to be < 0x80 and rejected otherwise. Set true to bypass
         * that check for genuinely binary content (firmware/bootloader) -
         * it's still sent unpacked, so it will most likely fail an MD5
         * mismatch at commit rather than corrupt anything (commit is
         * atomic).
         */
        bool allowBinary = false;
        /**
         * @brief Reply timeout for the final commit step, in milliseconds.
         *
         * Commit validates every file's MD5 on-device before applying
         * anything, which can take much longer than any other command for a
         * large file - give it its own generous timeout rather than
         * ConnectOptions::timeoutMs (meant for quick request/reply commands).
         * Bump this further for very large files if a commit still times
         * out.
         */
        int commitTimeoutMs = 60000;
        /// Called after each chunk is acknowledged, with (bytesSentSoFar,
        /// totalBytes).
        std::function<void(size_t, size_t)> onProgress;
    };

    /**
     * @brief One-line human-readable rendering of a Response, e.g.
     * "Pot Touch: pot=3 control=1042 touched=1" or "ACK" or "NACK". Useful
     * for logging whatever comes back from Client::poll(). Mirrors the
     * electraone CLI's `events listen`.
     * @param response Response to describe.
     * @return The rendered description.
     */
    std::string describeEvent(const Response &response);

    class Client
    {
    public:
        Client();
        ~Client();
        Client(const Client &) = delete;
        Client &operator=(const Client &) = delete;

        /**
         * @brief Enumerates MIDI output port names, for building your own
         * port picker instead of relying on ConnectOptions::portNameSubstring.
         * @return The list of available MIDI output port names.
         */
        static std::vector<std::string> listOutputPorts();
        /// @copybrief listOutputPorts
        /// @return The list of available MIDI input port names.
        static std::vector<std::string> listInputPorts();

        /**
         * @brief Opens the matching MIDI port pair.
         * @param options Port selection and default timeout/transaction-id
         * options.
         * @throws std::runtime_error (with the candidate port list in
         * what()) if the match isn't exactly one port.
         */
        void connect(const ConnectOptions &options = {});
        /// @return True if connect() has succeeded and the port is open.
        bool isConnected() const;

        /// @name Low-level
        /// @{

        /**
         * @brief Sends an arbitrary category/command/payload and waits for a
         * reply.
         * @param category SysEx category byte.
         * @param command SysEx command byte.
         * @param params Payload bytes.
         * @param timeoutMs Reply timeout in milliseconds; < 0 uses the
         * ConnectOptions timeout.
         * @return The reply, or std::nullopt on timeout.
         */
        std::optional<Response> send(uint8_t category,
                                     uint8_t command,
                                     const std::vector<uint8_t> &params = {},
                                     int timeoutMs = -1);
        /**
         * @brief Sends a fully-formed message (including F0...F7) as-is.
         * @param fullMessage Complete raw message bytes.
         * @param timeoutMs Reply timeout in milliseconds; < 0 uses the
         * ConnectOptions timeout.
         * @return The reply, or std::nullopt on timeout.
         */
        std::optional<Response>
            sendRawBytes(const std::vector<uint8_t> &fullMessage,
                         int timeoutMs = -1);
        /**
         * @brief Waits for the next incoming message without sending
         * anything first - a reply to something already sent, or an
         * unsolicited event (Pot Touch, Page Switch, Log Message, Report
         * Progress, ...).
         * @param timeoutMs Timeout in milliseconds.
         * @return The message received, or std::nullopt on timeout.
         */
        std::optional<Response> poll(int timeoutMs);

        /// @}
        /// @name Info
        /// @{

        /// @brief Get Electra Info: firmware version, serial, hardware
        /// revision, model.
        std::optional<Response> getElectraInfo();
        /// @brief Get Runtime Info: free memory percentage, uptime.
        std::optional<Response> getRuntimeInfo();
        /// @brief Reboot the device.
        std::optional<Response> reboot();
        /// @brief Enable/disable device debugging.
        std::optional<Response> setDebug(bool enabled);
        /// @brief Enable/disable Control MIDI Learn mode.
        std::optional<Response> setMidiLearn(bool enabled);
        /// @brief Get USB Host Devices: connected USB MIDI devices.
        std::optional<Response> getUsbHostDevices();
        /// @brief Get Parameter Map: application parameter map entries.
        /// @note Not in docs.electra.one - found in the firmware source
        /// (ElectraCommand::Object::ParameterMap / SysexApi::sendParameterMap).
        std::optional<Response> getParameterMap();
        /// @brief Save Screenshot to the device.
        /// @note Not in docs.electra.one - found in the firmware source
        /// (ElectraCommand::Object::Screenshot / SysexApi::saveScreenshot).
        std::optional<Response> saveScreenshot();
        /**
         * @brief Sets up to 8 Lua breakpoint line numbers.
         *
         * @note Not in docs.electra.one - found in the firmware source
         * (SysexApi::processDebug, category 0x14/Object::Trace, sub-command
         * byte 91).
         *
         * @param breakpoints Line numbers to break on (at most 8).
         * @throws std::runtime_error if given more than 8.
         */
        std::optional<Response>
            setBreakpoints(const std::vector<uint16_t> &breakpoints);

        /// @}
        /// @name Preset
        /// @note getPreset/getLuaScript/getDeviceOverrides/getPersistedData:
        /// pass no bank/slot (or leave both nullopt) to query the current
        /// preset.
        /// @{

        /// @brief Get Preset JSON.
        std::optional<Response>
            getPreset(std::optional<int> bank = std::nullopt,
                      std::optional<int> slot = std::nullopt);
        /// @brief Upload Preset from JSON.
        std::optional<Response> uploadPreset(const std::string &presetJson);
        /// @brief Remove Preset.
        std::optional<Response> removePreset(int bank, int slot);
        /// @brief Clear Preset Slot.
        std::optional<Response> clearPresetSlot(int bank, int slot);
        /// @brief Get Preset List: all presets with metadata.
        std::optional<Response> getPresetList();
        /// @brief Get Preset Slot Info: metadata and file checksums.
        std::optional<Response> getPresetSlotInfo(int bank, int slot);
        /// @brief Switch Preset Slot (runtime, volatile).
        std::optional<Response> switchPresetSlot(int bank, int slot);
        /// @brief Set Preset Slot (runtime, volatile).
        std::optional<Response> setPresetSlot(int bank, int slot);
        /// @brief Reload Preset Slot.
        std::optional<Response> reloadPresetSlot();
        /// @brief Load Preloaded Preset (runtime, volatile).
        std::optional<Response>
            loadPreloadedPreset(int bank, int slot, const std::string &path);

        /// @}
        /// @name Lua
        /// @{

        /// @brief Get Lua Script source (omit bank/slot for the current
        /// preset).
        std::optional<Response>
            getLuaScript(std::optional<int> bank = std::nullopt,
                         std::optional<int> slot = std::nullopt);
        /// @brief Upload Lua Script from source.
        std::optional<Response> uploadLuaScript(const std::string &code);
        /// @brief Remove Lua Script.
        std::optional<Response> removeLuaScript(int bank, int slot);
        /// @brief Execute Lua Command (runtime, max 65535 bytes).
        std::optional<Response> executeLuaCommand(const std::string &code);

        /// @}
        /// @name Overrides / Persisted / Performance / Config
        /// @{

        /// @brief Get Device Overrides JSON (omit bank/slot for the current
        /// preset).
        std::optional<Response>
            getDeviceOverrides(std::optional<int> bank = std::nullopt,
                               std::optional<int> slot = std::nullopt);
        /// @brief Upload Device Overrides from JSON.
        std::optional<Response> uploadDeviceOverrides(const std::string &json);
        /// @brief Get Persisted Data: persisted Lua table as JSON (omit
        /// bank/slot for the current preset).
        std::optional<Response>
            getPersistedData(std::optional<int> bank = std::nullopt,
                             std::optional<int> slot = std::nullopt);
        /// @brief Upload Persisted Data from JSON.
        std::optional<Response> uploadPersistedData(const std::string &json);
        /// @brief Get Performance layout JSON.
        std::optional<Response> getPerformance(int bank, int slot);
        /// @brief Upload Performance layout from JSON.
        std::optional<Response> uploadPerformance(const std::string &json);
        /// @brief Get Configuration JSON.
        std::optional<Response> getConfiguration();
        /// @brief Upload Configuration from JSON.
        std::optional<Response> uploadConfiguration(const std::string &json);
        /// @brief Remove Config.
        std::optional<Response> removeConfig();

        /// @}
        /// @name Snapshot
        /// @{

        /// @brief Get Snapshots List for a project.
        std::optional<Response> getSnapshotsList(const std::string &projectId);
        /// @brief Get Snapshot Data (parameters) for a bank/slot.
        std::optional<Response>
            getSnapshotData(const std::string &projectId, int bank, int slot);
        /// @brief Update Snapshot name/color.
        std::optional<Response> updateSnapshot(const std::string &projectId,
                                               int bank,
                                               int slot,
                                               const std::string &name,
                                               const std::string &color);
        /// @brief Remove Snapshot.
        std::optional<Response>
            removeSnapshot(const std::string &projectId, int bank, int slot);
        /// @brief Swap two Snapshots.
        std::optional<Response> swapSnapshots(const std::string &projectId,
                                              int fromBank,
                                              int fromSlot,
                                              int toBank,
                                              int toSlot);
        /// @brief Set Snapshot Slot (runtime, volatile).
        std::optional<Response>
            setSnapshotSlot(const std::string &projectId, int bank, int slot);

        /// @}
        /**
         * @name Capture
         *
         * @note docs.electra.one gives Update/Remove/Swap Capture the same
         * category/command bytes as the Snapshot equivalents (a
         * documentation error) - these three use command byte 0x32
         * (CaptureInfo), confirmed against the firmware's SysexApi.cpp
         * dispatch. See README.md.
         */
        /// @{

        /// @brief Get Captures List for a project.
        std::optional<Response> getCapturesList(const std::string &projectId);
        /// @brief Get Capture Data (raw MIDI recording) for a bank/slot.
        std::optional<Response>
            getCaptureData(const std::string &projectId, int bank, int slot);
        /// @brief Update Capture name/color.
        std::optional<Response> updateCapture(const std::string &projectId,
                                              int bank,
                                              int slot,
                                              const std::string &name,
                                              const std::string &color);
        /// @brief Remove Capture.
        std::optional<Response>
            removeCapture(const std::string &projectId, int bank, int slot);
        /// @brief Swap two Captures.
        std::optional<Response> swapCaptures(const std::string &projectId,
                                             int fromBank,
                                             int fromSlot,
                                             int toBank,
                                             int toSlot);
        /// @brief Set Capture Slot (runtime, volatile).
        std::optional<Response>
            setCaptureSlot(const std::string &projectId, int bank, int slot);

        /// @}
        /// @name Control (runtime)
        /// @{

        /// @brief Update Control: name/color/visibility/displayed value
        /// text.
        std::optional<Response> updateControl(int id,
                                              const ControlUpdate &update);
        /// @brief Override Value Text (max 15 chars).
        std::optional<Response>
            overrideValueText(int id, int valueId, const std::string &text);

        /// @}
        /// @name UI (runtime)
        /// @{

        /// @brief Switch Page.
        std::optional<Response> switchPage(int page);
        /// @brief Switch Control Set.
        std::optional<Response> switchControlSet(int set);
        /// @brief Set Bottom Bar Text (max 40 chars).
        std::optional<Response> setBottomBarText(const std::string &text);

        /// @}
        /// @name Events / Logger / Window (runtime)
        /// @{

        /// @brief Set Events MIDI Port.
        std::optional<Response> setEventsMidiPort(Port port);
        /// @brief Subscribe Events: select which event types the device
        /// sends.
        std::optional<Response> subscribeEvents(const EventFlags &flags);
        /// @brief Control Logger Output: enable.
        std::optional<Response> enableLogger();
        /// @brief Control Logger Output: disable.
        std::optional<Response> disableLogger();
        /// @brief Set Logger MIDI Port.
        std::optional<Response> setLoggerMidiPort(Port port);
        /// @brief Stop window repaints.
        std::optional<Response> stopWindowRepaints();
        /// @brief Resume window repaints.
        std::optional<Response> resumeWindowRepaints();

        /// @}
        /**
         * @name File Transfer API (firmware 4.0+)
         *
         * Low-level primitives - for the common single-file case, prefer
         * uploadFile() below, which runs all four steps as one transaction.
         */
        /// @{

        /// @brief Open Cache Transaction: start a new file transfer session.
        std::optional<Response> openCacheTransaction();
        /// @brief Register Files: announce a file's ID and total size.
        std::optional<Response> registerFile(int id, uint32_t size);
        /**
         * @brief Transfer Chunks: send one chunk of file data and wait for
         * its ACK/NACK.
         * @param id File ID matching a prior registerFile() call.
         * @param data Chunk bytes.
         * @param allowBinary Skip the 7-bit ASCII check and send bytes as-is.
         * @param onProgress Invoked for any Report Progress events seen
         * while waiting (the device may interleave these before the chunk's
         * real ACK).
         */
        std::optional<Response> sendChunk(
            int id,
            const std::vector<uint8_t> &data,
            bool allowBinary = false,
            const std::function<void(const Response &)> &onProgress = {});
        /**
         * @brief Commit Transaction: finalize the transfer.
         *
         * Validates every file's MD5 on-device before applying anything,
         * which can take much longer than any other command for a large
         * file - the default timeout here (60s) is deliberately far more
         * generous than ConnectOptions::timeoutMs; pass a larger one for
         * very large files.
         *
         * @param commitJson File-commit JSON (see the File Transfer API
         * docs).
         * @param timeoutMs Reply timeout in milliseconds.
         */
        std::optional<Response> commitTransaction(const std::string &commitJson,
                                                  int timeoutMs = 60000);
        /// @brief Get Location Files: list files (name + MD5) at a storage
        /// location.
        std::optional<Response>
            getLocationFiles(const std::string &locationJson);
        /// @brief Remove Files from Location: delete every file at a
        /// storage location.
        std::optional<Response>
            removeLocationFiles(const std::string &locationJson);

        /// @}

        /**
         * @brief Uploads a file end-to-end: open + register + send all
         * chunks + commit, in one transaction.
         *
         * Runs open + register + all chunks + commit as one transaction,
         * computing the MD5 the commit step requires.
         *
         * @param content File bytes to upload.
         * @param options Destination and transfer options.
         * @return The commit step's response.
         * @throws std::runtime_error (naming the failed step) if any step is
         * NACKed or times out.
         */
        Response uploadFile(const std::vector<uint8_t> &content,
                            const UploadFileOptions &options);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace electraone
