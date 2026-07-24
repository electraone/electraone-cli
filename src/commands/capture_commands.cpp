#include "commands/command.hpp"
#include "commands/common.hpp"

// NOTE: the Electra docs give Remove/Update/Swap Capture the *exact same*
// category/command bytes as the corresponding Snapshot commands (0x05 0x06,
// 0x04 0x06, 0x06 0x06), with no distinguishing field in the JSON payload
// either. That's almost certainly a documentation copy/paste error - see
// README.md "Known documentation ambiguities". These three subcommands send
// the documented (probably wrong) bytes; use `electra raw` to override once
// you've confirmed the real values against a device or firmware source.

namespace {

std::vector<uint8_t> projectSlotJson(const std::string& projectId, int bank, int slot) {
    JsonDocument doc;
    doc["projectId"] = projectId;
    doc["bankNumber"] = bank;
    doc["slot"] = slot;
    return commands::jsonToBytes(doc);
}

}  // namespace

void registerCaptureCommands(CLI::App& app, runner::Context& ctx) {
    auto* capture = app.add_subcommand("capture", "Capture (recorded MIDI) operations");
    capture->require_subcommand(1);

    {
        static std::string projectId;
        auto* sub = capture->add_subcommand("list", "Get Captures List for a project");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        sub->callback([&ctx] {
            JsonDocument doc;
            doc["projectId"] = projectId;
            runner::runQuery(ctx, 0x02, 0x31, commands::jsonToBytes(doc));
        });
    }
    {
        static std::string projectId;
        static int bank = 0, slot = 0;
        auto* sub = capture->add_subcommand("get", "Get Capture Data (raw MIDI recording) for a bank/slot");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] { runner::runQuery(ctx, 0x02, 0x30, projectSlotJson(projectId, bank, slot)); });
    }
    {
        static std::string projectId, name, color;
        static int bank = 0, slot = 0;
        auto* sub = capture->add_subcommand(
            "update", "Update Capture name/color (bytes shared with Update Snapshot in the docs - see note above)");
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
            runner::runAction(ctx, 0x04, 0x06, commands::jsonToBytes(doc));
        });
    }
    {
        static std::string projectId;
        static int bank = 0, slot = 0;
        auto* sub =
            capture->add_subcommand("remove", "Remove Capture (bytes shared with Remove Snapshot in the docs - see note above)");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] { runner::runAction(ctx, 0x05, 0x06, projectSlotJson(projectId, bank, slot)); });
    }
    {
        static std::string projectId;
        static int fromBank = 0, fromSlot = 0, toBank = 0, toSlot = 0;
        auto* sub =
            capture->add_subcommand("swap", "Swap two Captures (bytes shared with Swap Snapshots in the docs - see note above)");
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
        auto* sub = capture->add_subcommand("set-slot", "Set Capture Slot (runtime, volatile)");
        sub->add_option("--project-id", projectId, "Project ID")->required();
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] { runner::runAction(ctx, 0x14, 0x33, projectSlotJson(projectId, bank, slot)); });
    }
}
