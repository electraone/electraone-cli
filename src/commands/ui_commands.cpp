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

void registerUiCommands(CLI::App &app, runner::Context &ctx)
{
    auto *ui = app.add_subcommand("ui", "Display/navigation operations");
    ui->require_subcommand(1);

    {
        static int page = 0;
        auto *sub = ui->add_subcommand("page-switch",
                                       "Switch Page (runtime, volatile)");
        sub->add_option("--page", page, "Page number")->required();
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::Switch,
                              ElectraCommand::Object::Page,
                              { static_cast<uint8_t>(page) });
        });
    }
    {
        static int set = 0;
        auto *sub = ui->add_subcommand(
            "control-set-switch", "Switch Control Set (runtime, volatile)");
        sub->add_option("--set", set, "Control set ID")->required();
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::Switch,
                              ElectraCommand::Object::ControlSet,
                              { static_cast<uint8_t>(set) });
        });
    }
    {
        static std::string text;
        auto *sub = ui->add_subcommand(
            "bottom-bar-text",
            "Set Bottom Bar Text (runtime, max 40 characters)");
        sub->add_option("text", text, "Text to display")->required();
        sub->callback([&ctx] {
            if (text.size() > 40) {
                throw std::runtime_error(
                    "bottom bar text must be at most 40 characters");
            }
            runner::runAction(ctx,
                              ElectraCommand::Type::UpdateRuntime,
                              ElectraCommand::Object::StatusBar,
                              sysex::encodeAscii(text));
        });
    }
}
