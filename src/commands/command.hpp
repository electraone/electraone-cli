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

#pragma once

namespace CLI
{
    class App;
}
namespace runner
{
    struct Context;
}

void registerInfoCommands(CLI::App &app, runner::Context &ctx);
void registerPresetCommands(CLI::App &app, runner::Context &ctx);
void registerLuaCommands(CLI::App &app, runner::Context &ctx);
void registerOverridesCommands(CLI::App &app, runner::Context &ctx);
void registerPersistedCommands(CLI::App &app, runner::Context &ctx);
void registerPerformanceCommands(CLI::App &app, runner::Context &ctx);
void registerConfigCommands(CLI::App &app, runner::Context &ctx);
void registerSnapshotCommands(CLI::App &app, runner::Context &ctx);
void registerCaptureCommands(CLI::App &app, runner::Context &ctx);
void registerControlCommands(CLI::App &app, runner::Context &ctx);
void registerUiCommands(CLI::App &app, runner::Context &ctx);
void registerEventsCommands(CLI::App &app, runner::Context &ctx);
void registerLoggerCommands(CLI::App &app, runner::Context &ctx);
void registerWindowCommands(CLI::App &app, runner::Context &ctx);
void registerFilesCommands(CLI::App &app, runner::Context &ctx);
void registerRawCommands(CLI::App &app, runner::Context &ctx);
