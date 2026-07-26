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

namespace
{
    std::vector<uint8_t>
        projectSlotJson(const std::string &projectId, int bank, int slot)
    {
        JsonDocument doc;
        doc["projectId"] = projectId;
        doc["bankNumber"] = bank;
        doc["slot"] = slot;
        return commands::jsonToBytes(doc);
    }

} // namespace

void registerCaptureCommands(CLI::App &app, runner::Context &ctx)
{
    auto *capture =
        app.add_subcommand("capture", "Capture (recorded MIDI) operations");
    capture->require_subcommand(1);

    {
        static std::string projectId;
        auto *sub =
            capture->add_subcommand("list", "Get Captures List for a project");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        sub->callback([&ctx] {
            JsonDocument doc;
            doc["projectId"] = projectId;
            runner::runQuery(ctx,
                             ElectraCommand::Type::FileRequest,
                             ElectraCommand::Object::CaptureList,
                             commands::jsonToBytes(doc));
        });
    }
    {
        static std::string projectId;
        static int bank = 0, slot = 0;
        auto *sub = capture->add_subcommand(
            "get", "Get Capture Data (raw MIDI recording) for a bank/slot");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runQuery(ctx,
                             ElectraCommand::Type::FileRequest,
                             ElectraCommand::Object::FileCapture,
                             projectSlotJson(projectId, bank, slot));
        });
    }
    {
        static std::string projectId, name, color;
        static int bank = 0, slot = 0;
        auto *sub =
            capture->add_subcommand("update", "Update Capture name/color");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->add_option("--name", name, "Capture name")->required();
        sub->add_option("--color", color, "Capture color")->required();
        sub->callback([&ctx] {
            JsonDocument doc;
            doc["projectId"] = projectId;
            doc["bankNumber"] = bank;
            doc["slot"] = slot;
            doc["name"] = name;
            doc["color"] = color;
            runner::runAction(ctx,
                              ElectraCommand::Type::Update,
                              ElectraCommand::Object::CaptureInfo,
                              commands::jsonToBytes(doc));
        });
    }
    {
        static std::string projectId;
        static int bank = 0, slot = 0;
        auto *sub = capture->add_subcommand("remove", "Remove Capture");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::Remove,
                              ElectraCommand::Object::CaptureInfo,
                              projectSlotJson(projectId, bank, slot));
        });
    }
    {
        static std::string projectId;
        static int fromBank = 0, fromSlot = 0, toBank = 0, toSlot = 0;
        auto *sub = capture->add_subcommand("swap", "Swap two Captures");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        sub->add_option("--from-bank", fromBank, "Source bank")->required();
        sub->add_option("--from-slot", fromSlot, "Source slot")->required();
        sub->add_option("--to-bank", toBank, "Destination bank")->required();
        sub->add_option("--to-slot", toSlot, "Destination slot")->required();
        sub->callback([&ctx] {
            JsonDocument doc;
            doc["projectId"] = projectId;
            doc["fromBankNumber"] = fromBank;
            doc["fromSlot"] = fromSlot;
            doc["toBankNumber"] = toBank;
            doc["toSlot"] = toSlot;
            runner::runAction(ctx,
                              ElectraCommand::Type::Swap,
                              ElectraCommand::Object::CaptureInfo,
                              commands::jsonToBytes(doc));
        });
    }
    {
        static std::string projectId;
        static int bank = 0, slot = 0;
        auto *sub = capture->add_subcommand(
            "set-slot", "Set Capture Slot (runtime, volatile)");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::UpdateRuntime,
                              ElectraCommand::Object::CaptureSlot,
                              projectSlotJson(projectId, bank, slot));
        });
    }
}
