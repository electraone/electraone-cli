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

void registerLoggerCommands(CLI::App &app, runner::Context &ctx)
{
    auto *logger = app.add_subcommand("logger", "Device log output operations");
    logger->require_subcommand(1);

    logger->add_subcommand("enable", "Control Logger Output: enable")
        ->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::SystemCall,
                              ElectraCommand::Object::Logger,
                              { 0x01, 0x00 });
        });
    logger->add_subcommand("disable", "Control Logger Output: disable")
        ->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::SystemCall,
                              ElectraCommand::Object::Logger,
                              { 0x00, 0x00 });
        });

    {
        static std::string port;
        auto *sub = logger->add_subcommand("set-port", "Set Logger MIDI Port");
        sub->add_option("--port", port, "port1, port2, or ctrl")->required();
        sub->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::UpdateRuntime,
                              ElectraCommand::Object::Logger,
                              { commands::parsePortSelector(port), 0x00 });
        });
    }
    {
        static std::string loggerPort = "ctrl";
        static int duration = 0;
        static bool page = false, controlSet = false, usbHost = false,
                    pots = false, touch = false, button = false, window = false,
                    all = true;
        auto *sub = logger->add_subcommand(
            "listen",
            "Enable logging, route it to this port, subscribe to controller "
            "events too, then print both (timestamped) until Ctrl+C or "
            "--duration elapses (default: all event types)");
        sub->add_option(
               "--logger-port",
               loggerPort,
               "Which device port to route log output to (default: ctrl, i.e. "
               "the port we're listening on)")
            ->default_val("ctrl");
        sub->add_option("--duration",
                        duration,
                        "Stop after this many seconds (0 = run forever)")
            ->default_val(0);
        commands::addSubscribeFlags(
            sub, page, controlSet, usbHost, pots, touch, button, window, all);
        sub->callback([&ctx] {
            auto enableMsg =
                sysex::buildMessage(ElectraCommand::Type::SystemCall,
                                    ElectraCommand::Object::Logger,
                                    { 0x01, 0x00 },
                                    ctx.txnId);
            auto setPortMsg = sysex::buildMessage(
                ElectraCommand::Type::UpdateRuntime,
                ElectraCommand::Object::Logger,
                { commands::parsePortSelector(loggerPort), 0x00 },
                ctx.txnId);
            uint8_t flags = commands::subscribeFlagsToByte(
                page, controlSet, usbHost, pots, touch, button, window, all);
            auto subscribeMsg =
                sysex::buildMessage(ElectraCommand::Type::UpdateRuntime,
                                    ElectraCommand::Object::EventSubscription,
                                    { flags },
                                    ctx.txnId);
            runner::listen(ctx,
                           { enableMsg, setPortMsg, subscribeMsg },
                           duration,
                           commands::printEventWithTimestamp);
        });
    }
}
