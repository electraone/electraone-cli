#pragma once

// Forward declarations for each command group's subcommand-registration
// function. Every register* function attaches its subcommands (and their
// callbacks) onto `app`, using `ctx` as the shared connection/output
// settings (see runner::Context).

namespace CLI {
class App;
}
namespace runner {
struct Context;
}

void registerInfoCommands(CLI::App& app, runner::Context& ctx);
void registerPresetCommands(CLI::App& app, runner::Context& ctx);
void registerLuaCommands(CLI::App& app, runner::Context& ctx);
void registerOverridesCommands(CLI::App& app, runner::Context& ctx);
void registerPersistedCommands(CLI::App& app, runner::Context& ctx);
void registerPerformanceCommands(CLI::App& app, runner::Context& ctx);
void registerConfigCommands(CLI::App& app, runner::Context& ctx);
void registerSnapshotCommands(CLI::App& app, runner::Context& ctx);
void registerCaptureCommands(CLI::App& app, runner::Context& ctx);
void registerControlCommands(CLI::App& app, runner::Context& ctx);
void registerUiCommands(CLI::App& app, runner::Context& ctx);
void registerEventsCommands(CLI::App& app, runner::Context& ctx);
void registerLoggerCommands(CLI::App& app, runner::Context& ctx);
void registerWindowCommands(CLI::App& app, runner::Context& ctx);
void registerFilesCommands(CLI::App& app, runner::Context& ctx);
void registerRawCommands(CLI::App& app, runner::Context& ctx);
