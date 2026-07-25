#include "commands/command.hpp"
#include "commands/common.hpp"

void registerOverridesCommands(CLI::App &app, runner::Context &ctx)
{
    auto *overrides =
        app.add_subcommand("overrides", "Device override operations");
    overrides->require_subcommand(1);

    {
        static int bank = 0, slot = 0;
        auto *sub = overrides->add_subcommand(
            "get",
            "Get Device Overrides JSON (omit bank/slot for the current preset)");
        auto bankSlotOpts = commands::addBankSlot(sub, bank, slot);
        CLI::Option *bOpt = bankSlotOpts.first, *sOpt = bankSlotOpts.second;
        sub->callback([&ctx, bOpt, sOpt] {
            runner::runQuery(
                ctx,
                0x02,
                0x0F,
                commands::optionalBankSlotParams(bOpt, sOpt, bank, slot));
        });
    }
    {
        static std::string file;
        auto *sub = overrides->add_subcommand(
            "upload",
            "Upload Device Overrides from a JSON file (use - for stdin)");
        sub->add_option("file", file, "Device overrides JSON file")->required();
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(file);
            runner::runAction(ctx, 0x01, 0x0F, sysex::encodeAscii(content));
        });
    }
}
