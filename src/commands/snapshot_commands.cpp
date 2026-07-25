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

void registerSnapshotCommands(CLI::App &app, runner::Context &ctx)
{
    auto *snapshot = app.add_subcommand("snapshot", "Snapshot operations");
    snapshot->require_subcommand(1);

    {
        static std::string projectId;
        auto *sub = snapshot->add_subcommand(
            "list", "Get Snapshots List for a project");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        sub->callback([&ctx] {
            JsonDocument doc;
            doc["projectId"] = projectId;
            runner::runQuery(ctx, 0x02, 0x05, commands::jsonToBytes(doc));
        });
    }
    {
        static std::string projectId;
        static int bank = 0, slot = 0;
        auto *sub = snapshot->add_subcommand(
            "get", "Get Snapshot Data (parameters) for a bank/slot");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runQuery(
                ctx, 0x02, 0x03, projectSlotJson(projectId, bank, slot));
        });
    }
    {
        static std::string projectId, name, color;
        static int bank = 0, slot = 0;
        auto *sub =
            snapshot->add_subcommand("update", "Update Snapshot name/color");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->add_option("--name", name, "Snapshot name")->required();
        sub->add_option("--color", color, "Snapshot color")->required();
        sub->callback([&ctx] {
            JsonDocument doc;
            doc["projectId"] = projectId;
            doc["bankNumber"] = bank;
            doc["slot"] = slot;
            doc["name"] = name;
            doc["color"] = color;
            runner::runAction(ctx, 0x04, 0x06, commands::jsonToBytes(doc));
        });
    }
    {
        static std::string projectId;
        static int bank = 0, slot = 0;
        auto *sub = snapshot->add_subcommand("remove", "Remove Snapshot");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(
                ctx, 0x05, 0x06, projectSlotJson(projectId, bank, slot));
        });
    }
    {
        static std::string projectId;
        static int fromBank = 0, fromSlot = 0, toBank = 0, toSlot = 0;
        auto *sub = snapshot->add_subcommand("swap", "Swap two Snapshots");
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
            runner::runAction(ctx, 0x06, 0x06, commands::jsonToBytes(doc));
        });
    }
    {
        static std::string projectId;
        static int bank = 0, slot = 0;
        auto *sub = snapshot->add_subcommand(
            "set-slot", "Set Snapshot Slot (runtime, volatile)");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(
                ctx, 0x14, 0x09, projectSlotJson(projectId, bank, slot));
        });
    }
}
