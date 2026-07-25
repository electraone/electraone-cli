#include "commands/command.hpp"
#include "commands/common.hpp"

void registerPerformanceCommands(CLI::App &app, runner::Context &ctx)
{
    auto *performance =
        app.add_subcommand("performance", "Performance layout operations");
    performance->require_subcommand(1);

    {
        static int bank = 0, slot = 0;
        auto *sub =
            performance->add_subcommand("get", "Get Performance layout JSON");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runQuery(
                ctx, 0x02, 0x11, commands::bankSlotParams(bank, slot));
        });
    }
    {
        static std::string file;
        auto *sub = performance->add_subcommand(
            "upload",
            "Upload Performance layout from a JSON file (use - for stdin)");
        sub->add_option("file", file, "Performance layout JSON file")
            ->required();
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(file);
            runner::runAction(ctx, 0x01, 0x11, sysex::encodeAscii(content));
        });
    }
}
