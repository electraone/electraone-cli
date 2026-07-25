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

#include <iostream>
#include <stdexcept>

#include "commands/command.hpp"
#include "commands/common.hpp"
#include "commands/event_decoder.hpp"
#include "commands/progress.hpp"
#include "commands/terminal.hpp"
#include "md5.hpp"

// File Transfer SysEx API (firmware 4.0+): https://docs.electra.one/developers/filetransfer.html
//
// Chunk encoding assumption: the docs describe Transfer Chunks' payload only
// as "MIDI 7-bit encoded", without giving a bit-packing algorithm for
// arbitrary 8-bit bytes (unlike, say, classic SysEx sample dumps). Every
// other payload in the wider Electra SysEx protocol is literal ASCII/7-bit
// bytes with no packing, so that's what `files upload` does by default:
// each source byte is sent as-is, and it's rejected up front if any byte is
// >= 0x80. This covers the realistic use cases (Lua scripts, JSON presets/
// config, deviceList, datafiles) since they're all text already. For
// genuinely binary content (firmware/bootloader) pass --allow-binary to
// bypass that check - bytes are still sent unpacked, so on real 8-bit binary
// data this will very likely fail with an MD5 mismatch at Commit rather
// than silently corrupting anything (Commit is atomic and verifies MD5
// before applying anything), but there's no verified encoding to fall back
// to. If you find the real packing scheme, this is the place to add it.

namespace
{

    std::vector<uint8_t> locationJson(const std::string &location,
                                      CLI::Option *bankOpt,
                                      int bank,
                                      CLI::Option *slotOpt,
                                      int slot,
                                      CLI::Option *nsOpt,
                                      const std::string &ns,
                                      CLI::Option *pathOpt,
                                      const std::string &path)
    {
        JsonDocument doc;
        doc["location"] = location;
        if (bankOpt->count() > 0)
            doc["bankNumber"] = bank;
        if (slotOpt->count() > 0)
            doc["slot"] = slot;
        if (nsOpt->count() > 0)
            doc["namespace"] = ns;
        if (pathOpt->count() > 0)
            doc["path"] = path;
        return commands::jsonToBytes(doc);
    }

    void printProgress(const sysex::ParsedResponse &r)
    {
        std::cout << commands::describeEvent(r) << "\n";
    }

} // namespace

void registerFilesCommands(CLI::App &app, runner::Context &ctx)
{
    auto *files = app.add_subcommand(
        "files",
        "File Transfer API: upload/list/remove files stored on the device (firmware 4.0+)");
    files->require_subcommand(1);

    files
        ->add_subcommand(
            "open", "Open Cache Transaction: start a new file transfer session")
        ->callback([&ctx] { runner::runAction(ctx, 0x01, 0x2D, {}); });

    {
        static int id = 0;
        static int size = 0;
        auto *sub = files->add_subcommand(
            "register", "Register Files: announce a file's ID and total size");
        sub->add_option("--id", id, "File ID (0-127)")->required();
        sub->add_option("--size", size, "Total file size in bytes")->required();
        sub->callback([&ctx] {
            auto sizeBytes = sysex::encode28bit(static_cast<uint32_t>(size));
            std::vector<uint8_t> params{ static_cast<uint8_t>(id) };
            params.insert(params.end(), sizeBytes.begin(), sizeBytes.end());
            runner::runAction(ctx, 0x01, 0x2E, params);
        });
    }
    {
        static int id = 0;
        static std::string data;
        static bool allowBinary = false;
        auto *sub = files->add_subcommand(
            "send-chunk", "Transfer Chunks: send one raw chunk of file data");
        sub->add_option("--id", id, "File ID matching a prior 'files register'")
            ->required();
        sub->add_option("--data",
                        data,
                        "File containing the chunk's bytes (use - for stdin)")
            ->required();
        sub->add_flag(
            "--allow-binary",
            allowBinary,
            "Skip the 7-bit ASCII check and send bytes as-is (see the note at the top of this file)");
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(data);
            std::vector<uint8_t> bytes(content.begin(), content.end());
            if (!allowBinary)
                bytes = sysex::encodeAscii(content);
            std::vector<uint8_t> params{ static_cast<uint8_t>(id) };
            params.insert(params.end(), bytes.begin(), bytes.end());
            runner::runActionAwaitingAck(
                ctx, 0x01, 0x2F, params, printProgress);
        });
    }
    {
        static std::string jsonFile;
        static int commitTimeoutMs = 60000;
        auto *sub = files->add_subcommand(
            "commit",
            "Commit Transaction: finalize the transfer using a hand-written "
            "file-commit JSON (use - for stdin)");
        sub->add_option("json", jsonFile, "File-commit JSON file")->required();
        sub->add_option(
               "--commit-timeout",
               commitTimeoutMs,
               "Reply timeout for this commit in milliseconds, independent of --timeout (on-device MD5 "
               "validation can take much longer than other commands for a large file)")
            ->default_val(60000);
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(jsonFile);
            runner::Context commitCtx = ctx;
            commitCtx.timeoutMs = commitTimeoutMs;
            runner::runAction(
                commitCtx, 0x04, 0x2D, sysex::encodeAscii(content));
        });
    }
    {
        static std::string location, ns, path;
        static int bank = 0, slot = 0;
        auto *sub = files->add_subcommand(
            "list",
            "Get Location Files: list files (name + MD5) at a storage location");
        sub->add_option("--location",
                        location,
                        "slots, updates, assets, modules, presets, or root")
            ->required();
        auto *bankOpt =
            sub->add_option("--bank", bank, "Bank number (location=slots)");
        auto *slotOpt =
            sub->add_option("--slot", slot, "Slot number (location=slots)");
        auto *nsOpt = sub->add_option(
            "--namespace", ns, "Namespace (location=modules/presets)");
        auto *pathOpt =
            sub->add_option("--path", path, "Path (location=modules/presets)");
        sub->callback([&ctx, bankOpt, slotOpt, nsOpt, pathOpt] {
            runner::runQuery(ctx,
                             0x02,
                             0x34,
                             locationJson(location,
                                          bankOpt,
                                          bank,
                                          slotOpt,
                                          slot,
                                          nsOpt,
                                          ns,
                                          pathOpt,
                                          path));
        });
    }
    {
        static std::string location, ns, path;
        static int bank = 0, slot = 0;
        auto *sub = files->add_subcommand(
            "remove",
            "Remove Files from Location: delete every file at a storage location");
        sub->add_option("--location",
                        location,
                        "slots, updates, assets, modules, presets, or root")
            ->required();
        auto *bankOpt =
            sub->add_option("--bank", bank, "Bank number (location=slots)");
        auto *slotOpt =
            sub->add_option("--slot", slot, "Slot number (location=slots)");
        auto *nsOpt = sub->add_option(
            "--namespace", ns, "Namespace (location=modules/presets)");
        auto *pathOpt =
            sub->add_option("--path", path, "Path (location=modules/presets)");
        sub->callback([&ctx, bankOpt, slotOpt, nsOpt, pathOpt] {
            runner::runAction(ctx,
                              0x05,
                              0x34,
                              locationJson(location,
                                           bankOpt,
                                           bank,
                                           slotOpt,
                                           slot,
                                           nsOpt,
                                           ns,
                                           pathOpt,
                                           path));
        });
    }

    {
        static std::string file, location, type, ns, path;
        static int bank = 0, slot = 0, id = 1, chunkSize = 256,
                   commitTimeoutMs = 60000;
        static bool allowBinary = false;
        static bool noProgress = false;
        auto *sub = files->add_subcommand(
            "upload",
            "Upload a file end-to-end: open + register + send all chunks + commit, in one transaction");
        sub->add_option("file", file, "File to upload (use - for stdin)")
            ->required();
        sub->add_option("--location",
                        location,
                        "slots, updates, assets, modules, presets, or root")
            ->required();
        sub->add_option(
               "--type",
               type,
               "firmware, bootloader, preset, lua, luaModule, ui, config, deviceList, datafile, or "
               "performance")
            ->required();
        auto *bankOpt =
            sub->add_option("--bank", bank, "Bank number (location=slots)");
        auto *slotOpt =
            sub->add_option("--slot", slot, "Slot number (location=slots)");
        auto *nsOpt = sub->add_option(
            "--namespace", ns, "Namespace (location=modules/presets)");
        auto *pathOpt =
            sub->add_option("--path", path, "Path (location=modules/presets)");
        sub->add_option("--id", id, "File ID for this transaction")
            ->default_val(1);
        sub->add_option("--chunk-size",
                        chunkSize,
                        "Bytes of file data per Transfer Chunks message")
            ->default_val(256);
        sub->add_flag(
            "--allow-binary",
            allowBinary,
            "Skip the 7-bit ASCII check and send bytes as-is (see the note at the top of this file)");
        sub->add_option(
               "--commit-timeout",
               commitTimeoutMs,
               "Reply timeout for the final commit in milliseconds, independent of --timeout (on-device "
               "MD5 validation can take much longer than opening/registering/sending chunks)")
            ->default_val(60000);
        sub->add_flag("--no-progress",
                      noProgress,
                      "Suppress the progress bar shown while sending chunks");

        sub->callback([&ctx, bankOpt, slotOpt, nsOpt, pathOpt] {
            auto content = runner::readFileOrStdin(file);
            if (chunkSize <= 0)
                throw std::runtime_error("--chunk-size must be positive");

            if (!allowBinary) {
                for (unsigned char c : content) {
                    if (c >= 0x80) {
                        throw std::runtime_error(
                            "file contains a byte >= 0x80; the chunk encoding for real binary data isn't "
                            "verified (see the note at the top of files_commands.cpp) - pass --allow-binary "
                            "to attempt it anyway");
                    }
                }
            }

            std::string md5 = md5Hex(content);
            std::cout << "MD5: " << md5 << " (" << content.size()
                      << " bytes)\n";

            std::cout << "Opening cache transaction...\n";
            runner::runAction(ctx, 0x01, 0x2D, {});
            if (runner::exitCode != 0)
                return;

            std::cout << "Registering file id " << id << "...\n";
            auto sizeBytes =
                sysex::encode28bit(static_cast<uint32_t>(content.size()));
            std::vector<uint8_t> registerParams{ static_cast<uint8_t>(id) };
            registerParams.insert(
                registerParams.end(), sizeBytes.begin(), sizeBytes.end());
            runner::runAction(ctx, 0x01, 0x2E, registerParams);
            if (runner::exitCode != 0)
                return;

            // A live-updating bar only makes sense on a real terminal; with
            // --no-progress, or when stdout is redirected/piped, print
            // nothing per-chunk at all (rather than falling back to the old
            // one-line-per-chunk spam) - the MD5/step/summary lines around
            // this loop are enough to follow along either way.
            bool showProgress = !noProgress && commands::isStdoutTerminal();

            size_t sent = 0;
            while (sent < content.size()) {
                size_t len = std::min(static_cast<size_t>(chunkSize),
                                      content.size() - sent);
                std::vector<uint8_t> chunkParams{ static_cast<uint8_t>(id) };
                chunkParams.insert(chunkParams.end(),
                                   content.begin() + static_cast<long>(sent),
                                   content.begin()
                                       + static_cast<long>(sent + len));
                // Report Progress events aren't surfaced here (unlike
                // `files send-chunk`) - they'd print a line in between our
                // own in-place bar updates and break it up.
                runner::runActionAwaitingAck(
                    ctx, 0x01, 0x2F, chunkParams, nullptr);
                if (runner::exitCode != 0)
                    return;
                sent += len;
                if (showProgress) {
                    std::cout
                        << "\r"
                        << commands::formatProgressBar(sent, content.size())
                        << std::flush;
                }
            }
            if (showProgress)
                std::cout << "\n";

            std::cout << "Committing...\n";
            JsonDocument doc;
            auto filesArray = doc["files"].to<JsonArray>();
            auto fileObj = filesArray.add<JsonObject>();
            fileObj["id"] = id;
            fileObj["location"] = location;
            fileObj["type"] = type;
            if (bankOpt->count() > 0)
                fileObj["bankNumber"] = bank;
            if (slotOpt->count() > 0)
                fileObj["slot"] = slot;
            if (nsOpt->count() > 0)
                fileObj["namespace"] = ns;
            if (pathOpt->count() > 0)
                fileObj["path"] = path;
            fileObj["md5"] = md5;
            runner::Context commitCtx = ctx;
            commitCtx.timeoutMs = commitTimeoutMs;
            runner::runAction(
                commitCtx, 0x04, 0x2D, commands::jsonToBytes(doc));
            if (runner::exitCode != 0)
                return;

            std::cout << "Uploaded " << file << " -> " << location << "/"
                      << type << " (id " << id << ")\n";
        });
    }
}
