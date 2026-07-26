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

void registerConfigCommands(CLI::App &app, runner::Context &ctx)
{
    auto *config =
        app.add_subcommand("config", "System configuration operations");
    config->require_subcommand(1);

    config->add_subcommand("get", "Get Configuration JSON")->callback([&ctx] {
        runner::runQuery(ctx,
                         ElectraCommand::Type::FileRequest,
                         ElectraCommand::Object::FileConfig,
                         {});
    });

    {
        static std::string file;
        auto *sub = config->add_subcommand(
            "upload",
            "Upload Configuration from a JSON file (use - for stdin)");
        sub->add_option("file", file, "Configuration JSON file")->required();
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(file);
            runner::runAction(ctx,
                              ElectraCommand::Type::FileUpload,
                              ElectraCommand::Object::FileConfig,
                              sysex::encodeAscii(content));
        });
    }
    config->add_subcommand("remove", "Remove Config")->callback([&ctx] {
        runner::runAction(ctx,
                          ElectraCommand::Type::Remove,
                          ElectraCommand::Object::FileConfig,
                          {});
    });
}
