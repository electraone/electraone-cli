#include <stdexcept>

#include "commands/command.hpp"
#include "commands/common.hpp"

void registerLuaCommands(CLI::App& app, runner::Context& ctx) {
    auto* lua = app.add_subcommand("lua", "Lua script operations");
    lua->require_subcommand(1);

    {
        static int bank = 0, slot = 0;
        auto* sub = lua->add_subcommand("get", "Get Lua Script source (omit bank/slot for the current preset)");
        auto bankSlotOpts = commands::addBankSlot(sub, bank, slot);
        CLI::Option *bOpt = bankSlotOpts.first, *sOpt = bankSlotOpts.second;
        sub->callback([&ctx, bOpt, sOpt] {
            runner::runQuery(ctx, 0x02, 0x0C, commands::optionalBankSlotParams(bOpt, sOpt, bank, slot));
        });
    }
    {
        static std::string file;
        auto* sub = lua->add_subcommand("upload", "Upload Lua Script from a source file (use - for stdin)");
        sub->add_option("file", file, "Lua source file")->required();
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(file);
            runner::runAction(ctx, 0x01, 0x0C, sysex::encodeAscii(content));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto* sub = lua->add_subcommand("remove", "Remove Lua Script");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] { runner::runAction(ctx, 0x05, 0x0C, commands::bankSlotParams(bank, slot)); });
    }
    {
        static std::string code;
        static std::string file;
        auto* sub = lua->add_subcommand("exec", "Execute Lua Command (max 65535 bytes)");
        sub->add_option("code", code, "Lua code to execute");
        sub->add_option("--file", file, "Read the Lua code from this file instead (use - for stdin)");
        sub->callback([&ctx] {
            std::string source = !file.empty() ? runner::readFileOrStdin(file) : code;
            if (source.empty()) {
                throw std::runtime_error("provide Lua code as an argument or via --file");
            }
            runner::runAction(ctx, 0x08, 0x0D, sysex::encodeAscii(source));
        });
    }
}
