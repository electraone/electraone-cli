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
