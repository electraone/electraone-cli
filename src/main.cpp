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

#include <CLI/CLI.hpp>
#include <iostream>

#include "commands/command.hpp"
#include "commands/common.hpp"
#include "commands/runner.hpp"
#include "midi_transport.hpp"

int main(int argc, char **argv)
{
    CLI::App app{
        "electraone - command-line suite for the Electra One SysEx protocol\n"
        "https://docs.electra.one/developers/midiimplementation.html"
    };
    app.require_subcommand(1);

    auto *listPortsCmd = app.add_subcommand(
        "list-ports", "List available MIDI input/output ports");
    listPortsCmd->callback([] { MidiTransport::listPorts(); });

    runner::Context ctx;
    commands::addGlobalOptions(app, ctx);

    registerInfoCommands(app, ctx);
    registerPresetCommands(app, ctx);
    registerLuaCommands(app, ctx);
    registerOverridesCommands(app, ctx);
    registerPersistedCommands(app, ctx);
    registerPerformanceCommands(app, ctx);
    registerConfigCommands(app, ctx);
    registerSnapshotCommands(app, ctx);
    registerCaptureCommands(app, ctx);
    registerControlCommands(app, ctx);
    registerUiCommands(app, ctx);
    registerEventsCommands(app, ctx);
    registerLoggerCommands(app, ctx);
    registerWindowCommands(app, ctx);
    registerFilesCommands(app, ctx);
    registerRawCommands(app, ctx);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }

    return runner::exitCode;
}
