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

/**
 * @file command.hpp
 *
 * @brief Declarations for each command group's registration function. Each
 * `register*Commands` function adds one top-level subcommand (and its
 * children) to app, wiring their callbacks to runner::runQuery/runAction via
 * the shared ctx. Defined one per "_commands.cpp" file under src/commands/.
 */

#pragma once

namespace CLI
{
    class App;
}
namespace runner
{
    struct Context;
}

/// @brief Registers `info`, `debug`, `midi-learn`, `usb-devices`,
/// `parameter-map`, and `screenshot`.
void registerInfoCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `preset` command group.
void registerPresetCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `lua` command group.
void registerLuaCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `overrides` command group.
void registerOverridesCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `persisted` command group.
void registerPersistedCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `performance` command group.
void registerPerformanceCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `config` command group.
void registerConfigCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `snapshot` command group.
void registerSnapshotCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `capture` command group.
void registerCaptureCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `control` command group.
void registerControlCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `ui` command group.
void registerUiCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `events` command group.
void registerEventsCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `logger` command group.
void registerLoggerCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `window` command group.
void registerWindowCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `files` command group (File Transfer API).
void registerFilesCommands(CLI::App &app, runner::Context &ctx);
/// @brief Registers the `raw` escape-hatch command.
void registerRawCommands(CLI::App &app, runner::Context &ctx);
