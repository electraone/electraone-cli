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

// info, runtime-info, reboot, debug, midi-learn, usb-devices

void registerInfoCommands(CLI::App &app, runner::Context &ctx)
{
    app.add_subcommand(
           "info",
           "Get Electra Info: firmware version, serial, hardware revision, model")
        ->callback([&ctx] { runner::runQuery(ctx, 0x02, 0x7F, {}); });

    app.add_subcommand("runtime-info",
                       "Get Runtime Info: free memory percentage, uptime")
        ->callback([&ctx] { runner::runQuery(ctx, 0x02, 0x7E, {}); });

    app.add_subcommand("reboot", "Reboot the device")->callback([&ctx] {
        runner::runAction(ctx, 0x7F, 0x78, {});
    });

    auto *debug =
        app.add_subcommand("debug", "Enable/disable device debugging");
    debug->require_subcommand(1);
    debug->add_subcommand("enable", "Enable debugging")->callback([&ctx] {
        runner::runAction(ctx, 0x7C, 0x01, {});
    });
    debug->add_subcommand("disable", "Disable debugging")->callback([&ctx] {
        runner::runAction(ctx, 0x7C, 0x00, {});
    });

    auto *midiLearn =
        app.add_subcommand("midi-learn", "Control MIDI Learn mode");
    midiLearn->require_subcommand(1);
    midiLearn->add_subcommand("enable", "Enable MIDI Learn")->callback([&ctx] {
        runner::runAction(ctx, 0x03, 0x01, {});
    });
    midiLearn->add_subcommand("disable", "Disable MIDI Learn")
        ->callback([&ctx] { runner::runAction(ctx, 0x03, 0x00, {}); });
    static int duration = 0;
    auto *listen = midiLearn->add_subcommand(
        "listen",
        "Listen for MIDI Learn Info events (parameter captures) until Ctrl+C or --duration elapses");
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
        ->callback([&ctx] { runner::runQuery(ctx, 0x02, 0x10, {}); });
}
