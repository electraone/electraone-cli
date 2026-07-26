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

#include <stdexcept>

#include "commands/command.hpp"
#include "commands/common.hpp"
#include "electra_command.hpp"

void registerLuaCommands(CLI::App &app, runner::Context &ctx)
{
    auto *lua = app.add_subcommand("lua", "Lua script operations");
    lua->require_subcommand(1);

    {
        static int bank = 0, slot = 0;
        auto *sub = lua->add_subcommand(
            "get",
            "Get Lua Script source (omit bank/slot for the current preset)");
        auto bankSlotOpts = commands::addBankSlot(sub, bank, slot);
        CLI::Option *bOpt = bankSlotOpts.first, *sOpt = bankSlotOpts.second;
        sub->callback([&ctx, bOpt, sOpt] {
            runner::runQuery(
                ctx,
                ElectraCommand::Type::FileRequest,
                ElectraCommand::Object::FileLua,
                commands::optionalBankSlotParams(bOpt, sOpt, bank, slot));
        });
    }
    {
        static std::string file;
        auto *sub = lua->add_subcommand(
            "upload", "Upload Lua Script from a source file (use - for stdin)");
        sub->add_option("file", file, "Lua source file")->required();
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(file);
            runner::runAction(ctx,
                              ElectraCommand::Type::FileUpload,
                              ElectraCommand::Object::FileLua,
                              sysex::encodeAscii(content));
        });
    }
    {
        static int bank = 0, slot = 0;
        auto *sub = lua->add_subcommand("remove", "Remove Lua Script");
        commands::addBankSlot(sub, bank, slot, true);
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::Remove,
                              ElectraCommand::Object::FileLua,
                              commands::bankSlotParams(bank, slot));
        });
    }
    {
        static std::string code;
        static std::string file;
        auto *sub = lua->add_subcommand(
            "exec", "Execute Lua Command (max 65535 bytes)");
        sub->add_option("code", code, "Lua code to execute");
        sub->add_option(
            "--file",
            file,
            "Read the Lua code from this file instead (use - for stdin)");
        sub->callback([&ctx] {
            std::string source =
                !file.empty() ? runner::readFileOrStdin(file) : code;
            if (source.empty()) {
                throw std::runtime_error(
                    "provide Lua code as an argument or via --file");
            }
            runner::runAction(ctx,
                              ElectraCommand::Type::Execute,
                              ElectraCommand::Object::Function,
                              sysex::encodeAscii(source));
        });
    }
}
