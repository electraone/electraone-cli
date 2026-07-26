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

void registerWindowCommands(CLI::App &app, runner::Context &ctx)
{
    auto *window = app.add_subcommand("window", "Control Window Repaints");
    window->require_subcommand(1);

    window->add_subcommand("stop", "Stop window repaints")->callback([&ctx] {
        runner::runAction(ctx,
                          ElectraCommand::Type::SystemCall,
                          ElectraCommand::Object::Window,
                          { 0x00, 0x00 });
    });
    window->add_subcommand("resume", "Resume window repaints")
        ->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::SystemCall,
                              ElectraCommand::Object::Window,
                              { 0x01, 0x00 });
        });
}
