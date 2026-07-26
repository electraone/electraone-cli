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

void registerInfoCommands(CLI::App &app, runner::Context &ctx)
{
    app.add_subcommand(
           "info",
           "Get Electra Info: firmware version, serial, hardware revision, "
           "model")
        ->callback([&ctx] {
            runner::runQuery(ctx,
                             ElectraCommand::Type::FileRequest,
                             ElectraCommand::Object::ElectraInfo,
                             {});
        });

    app.add_subcommand("runtime-info",
                       "Get Runtime Info: free memory percentage, uptime")
        ->callback([&ctx] {
            runner::runQuery(ctx,
                             ElectraCommand::Type::FileRequest,
                             ElectraCommand::Object::RuntimeInfo,
                             {});
        });

    app.add_subcommand("reboot", "Reboot the device")->callback([&ctx] {
        runner::runAction(ctx,
                          ElectraCommand::Type::SystemCall,
                          ElectraCommand::Object::Reboot,
                          {});
    });

    auto *debug =
        app.add_subcommand("debug", "Enable/disable device debugging");
    debug->require_subcommand(1);
    // Category 0x7C doesn't correspond to any ElectraCommand::Type value; it
    // numerically matches Object::AppInfo instead (see the same note on
    // Client::setDebug in ElectraOneClient.cpp). 0x01/0x00 are plain booleans,
    // not ElectraCommand::Object::Enable/Disable (those are 0x01/0x02).
    debug->add_subcommand("enable", "Enable debugging")->callback([&ctx] {
        runner::runAction(ctx, ElectraCommand::Object::AppInfo, 0x01, {});
    });
    debug->add_subcommand("disable", "Disable debugging")->callback([&ctx] {
        runner::runAction(ctx, ElectraCommand::Object::AppInfo, 0x00, {});
    });
    {
        static std::vector<int> breakpoints;
        auto *sub = debug->add_subcommand(
            "set-breakpoints",
            "Set up to 8 Lua breakpoint line numbers (not in docs.electra.one "
            "- found in the firmware source)");
        sub->add_option("--breakpoints",
                        breakpoints,
                        "Line numbers, e.g. --breakpoints 10 42 100")
            ->required();
        sub->callback([&ctx] {
            if (breakpoints.size() > 8) {
                throw std::runtime_error(
                    "set-breakpoints: at most 8 breakpoints are supported");
            }
            JsonDocument doc;
            auto arr = doc.to<JsonArray>();
            for (int bp : breakpoints)
                arr.add(bp);
            std::vector<uint8_t> params{
                ElectraCommand::Trace::SetBreakpoints
            };
            auto jsonBytes = commands::jsonToBytes(doc);
            params.insert(params.end(), jsonBytes.begin(), jsonBytes.end());
            runner::runAction(ctx,
                              ElectraCommand::Type::UpdateRuntime,
                              ElectraCommand::Object::Trace,
                              params);
        });
    }

    auto *midiLearn =
        app.add_subcommand("midi-learn", "Control MIDI Learn mode");
    midiLearn->require_subcommand(1);
    midiLearn->add_subcommand("enable", "Enable MIDI Learn")->callback([&ctx] {
        runner::runAction(ctx,
                          ElectraCommand::Type::MidiLearnSwitch,
                          ElectraCommand::Object::MidiLearnOn,
                          {});
    });
    midiLearn->add_subcommand("disable", "Disable MIDI Learn")
        ->callback([&ctx] {
            runner::runAction(ctx,
                              ElectraCommand::Type::MidiLearnSwitch,
                              ElectraCommand::Object::MidiLearnOff,
                              {});
        });
    static int duration = 0;
    auto *listen = midiLearn->add_subcommand(
        "listen",
        "Listen for MIDI Learn Info events (parameter captures) until Ctrl+C "
        "or --duration elapses");
    listen
        ->add_option("--duration",
                     duration,
                     "Stop after this many seconds (0 = run forever)")
        ->default_val(0);
    listen->callback([&ctx] {
        runner::listen(ctx, {}, duration, commands::printEventWithTimestamp);
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
