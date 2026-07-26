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

#include "commands/command.hpp"
#include "commands/common.hpp"
#include "electra_command.hpp"

void registerPresetCommands(CLI::App &app, runner::Context &ctx)
{
    auto *preset = app.add_subcommand("preset", "Preset operations");
    preset->require_subcommand(1);

    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand(
            "get", "Get Preset JSON (omit bank/slot for the current preset)");
        auto bankSlotOpts = commands::addBankSlot(sub, bank, slot);
        CLI::Option *bOpt = bankSlotOpts.first, *sOpt = bankSlotOpts.second;
        sub->callback([&ctx, bOpt, sOpt] {
            runner::runQuery(
                ctx,
                ElectraCommand::Type::FileRequest,
                ElectraCommand::Object::FilePreset,
                commands::optionalBankSlotParams(bOpt, sOpt, bank, slot));
        });
    }
    {
        static std::string file;
        auto *sub = preset->add_subcommand(
            "upload", "Upload Preset from a JSON file (use - for stdin)");
        sub->add_option("file", file, "Preset JSON file")->required();
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(file);
            runner::runAction(ctx,
                              ElectraCommand::Type::FileUpload,
                              ElectraCommand::Object::FilePreset,
                              sysex::encodeAscii(content));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand("remove", "Remove Preset");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::Remove,
                              ElectraCommand::Object::FilePreset,
                              commands::bankSlotParams(bank, slot));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand("clear-slot", "Clear Preset Slot");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::Remove,
                              ElectraCommand::Object::PresetSlot,
                              commands::bankSlotParams(bank, slot));
        });
    }
    preset->add_subcommand("list", "Get Preset List: all presets with metadata")
        ->callback([&ctx] {
            runner::runQuery(ctx,
                             ElectraCommand::Type::FileRequest,
                             ElectraCommand::Object::PresetList,
                             {});
        });
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand(
            "slot-info", "Get Preset Slot Info: metadata and file checksums");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runQuery(ctx,
                             ElectraCommand::Type::FileRequest,
                             ElectraCommand::Object::PresetSlot,
                             commands::bankSlotParams(bank, slot));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand(
            "switch", "Switch Preset Slot (runtime, volatile)");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::Switch,
                              ElectraCommand::Object::PresetSlot,
                              commands::bankSlotParams(bank, slot));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand(
            "set-slot", "Set Preset Slot (runtime, volatile)");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::UpdateRuntime,
                              ElectraCommand::Object::PresetSlot,
                              commands::bankSlotParams(bank, slot));
        });
    }
    preset->add_subcommand("reload", "Reload Preset Slot")->callback([&ctx] {
        runner::runAction(ctx,
                          ElectraCommand::Type::Execute,
                          ElectraCommand::Object::PresetSlot,
                          {});
    });
    {
        static int bank = 0, slot = 0;
        static std::string path;
        auto *sub = preset->add_subcommand(
            "load-preloaded", "Load Preloaded Preset (runtime, volatile)");
        commands::addBankSlot(sub, bank, slot, true);
        sub->add_option("--path", path, "Preloaded preset path")->required();
        sub->callback([&ctx] {
            JsonDocument doc;
            doc["bankNumber"] = bank;
            doc["slot"] = slot;
            doc["preset"] = path;
            runner::runAction(ctx,
                              ElectraCommand::Type::Update,
                              ElectraCommand::Object::PresetSlot,
                              commands::jsonToBytes(doc));
        });
    }
}
