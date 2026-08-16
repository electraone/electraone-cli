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
#include "commands/event_decoder.hpp"
#include "electra_command.hpp"

void registerSystemCommands(CLI::App &app, runner::Context &ctx)
{
    app.add_subcommand("reboot", "Reboot the device")->callback([&ctx] {
        runner::runAction(ctx,
                          ElectraCommand::Type::SystemCall,
                          ElectraCommand::Object::Reboot,
                          {});
    });

    auto *usb = app.add_subcommand("usb-devices", "USB host device queries");
    usb->require_subcommand(1);
    usb->add_subcommand("list",
                        "Get USB Host Devices: connected USB MIDI devices")
        ->callback([&ctx] {
            runner::runQuery(ctx,
                             ElectraCommand::Type::FileRequest,
                             ElectraCommand::Object::UsbHostList,
                             {});
        });

    auto *parameterMap = app.add_subcommand(
        "parameter-map", "Application parameter map queries");
    parameterMap->require_subcommand(1);
    parameterMap
        ->add_subcommand("list",
                         "Get Parameter Map: application parameter map entries")
        ->callback([&ctx] {
            runner::runQuery(ctx,
                             ElectraCommand::Type::FileRequest,
                             ElectraCommand::Object::ParameterMap,
                             {});
        });

    app.add_subcommand("screenshot", "Save Screenshot to the device")
        ->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::SystemCall,
                              ElectraCommand::Object::Screenshot,
                              {});
        });
}
