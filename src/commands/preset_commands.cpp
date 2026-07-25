#include "commands/command.hpp"
#include "commands/common.hpp"

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
                0x02,
                0x01,
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
            runner::runAction(ctx, 0x01, 0x01, sysex::encodeAscii(content));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand("remove", "Remove Preset");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(
                ctx, 0x05, 0x01, commands::bankSlotParams(bank, slot));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand("clear-slot", "Clear Preset Slot");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(
                ctx, 0x05, 0x08, commands::bankSlotParams(bank, slot));
        });
    }
    preset->add_subcommand("list", "Get Preset List: all presets with metadata")
        ->callback([&ctx] { runner::runQuery(ctx, 0x02, 0x04, {}); });
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand(
            "slot-info", "Get Preset Slot Info: metadata and file checksums");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runQuery(
                ctx, 0x02, 0x08, commands::bankSlotParams(bank, slot));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand(
            "switch", "Switch Preset Slot (runtime, volatile)");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(
                ctx, 0x09, 0x08, commands::bankSlotParams(bank, slot));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto *sub = preset->add_subcommand(
            "set-slot", "Set Preset Slot (runtime, volatile)");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(
                ctx, 0x14, 0x08, commands::bankSlotParams(bank, slot));
        });
    }
    preset->add_subcommand("reload", "Reload Preset Slot")->callback([&ctx] {
        runner::runAction(ctx, 0x08, 0x08, {});
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
            doc["path"] = path;
            runner::runAction(ctx, 0x04, 0x08, commands::jsonToBytes(doc));
        });
    }
}
