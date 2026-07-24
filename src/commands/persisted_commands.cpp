#include "commands/command.hpp"
#include "commands/common.hpp"

void registerPersistedCommands(CLI::App& app, runner::Context& ctx) {
    auto* persisted = app.add_subcommand("persisted", "Persisted Lua data operations");
    persisted->require_subcommand(1);

    {
        static int bank = 0, slot = 0;
        auto* sub = persisted->add_subcommand(
            "get", "Get Persisted Data: persisted Lua table as JSON (omit bank/slot for the current preset)");
        auto bankSlotOpts = commands::addBankSlot(sub, bank, slot);
        CLI::Option *bOpt = bankSlotOpts.first, *sOpt = bankSlotOpts.second;
        sub->callback([&ctx, bOpt, sOpt] {
            runner::runQuery(ctx, 0x02, 0x12, commands::optionalBankSlotParams(bOpt, sOpt, bank, slot));
        });
    }
    {
        static std::string file;
        auto* sub = persisted->add_subcommand("upload", "Upload Persisted Data from a JSON file (use - for stdin)");
        sub->add_option("file", file, "Persisted data JSON file")->required();
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(file);
            runner::runAction(ctx, 0x01, 0x12, sysex::encodeAscii(content));
        });
    }
}
