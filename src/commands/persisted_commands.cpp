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

void registerPersistedCommands(CLI::App &app, runner::Context &ctx)
{
    auto *persisted =
        app.add_subcommand("persisted", "Persisted Lua data operations");
    persisted->require_subcommand(1);

    {
        static int bank = 0, slot = 0;
        auto *sub = persisted->add_subcommand(
            "get",
            "Get Persisted Data: persisted Lua table as JSON "
            "(omit bank/slot for the current preset)");
        auto bankSlotOpts = commands::addBankSlot(sub, bank, slot);
        CLI::Option *bOpt = bankSlotOpts.first, *sOpt = bankSlotOpts.second;
        sub->callback([&ctx, bOpt, sOpt] {
            runner::runQuery(
                ctx,
                ElectraCommand::Type::FileRequest,
                ElectraCommand::Object::Datafile,
                commands::optionalBankSlotParams(bOpt, sOpt, bank, slot));
        });
    }
    {
        static std::string file;
        auto *sub = persisted->add_subcommand(
            "upload",
            "Upload Persisted Data from a JSON file (use - for stdin)");
        sub->add_option("file", file, "Persisted data JSON file")->required();
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(file);
            runner::runAction(ctx,
                              ElectraCommand::Type::FileUpload,
                              ElectraCommand::Object::Datafile,
                              sysex::encodeAscii(content));
        });
    }
}
