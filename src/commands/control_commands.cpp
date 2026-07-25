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

void registerControlCommands(CLI::App &app, runner::Context &ctx)
{
    auto *control = app.add_subcommand("control", "Live control operations");
    control->require_subcommand(1);

    {
        static int id = 0;
        static std::string name, color;
        static int value = 0;
        static bool visible = false;
        auto *sub = control->add_subcommand(
            "update", "Update Control: name/color/visibility/value (runtime)");
        sub->add_option("--id", id, "Control ID")->required();
        auto *nameOpt = sub->add_option("--name", name, "New control name");
        auto *colorOpt = sub->add_option("--color", color, "New control color");
        auto *visibleOpt = sub->add_flag(
            "--visible,!--hidden", visible, "Show or hide the control");
        auto *valueOpt = sub->add_option("--value", value, "New control value");
        sub->callback([&ctx, nameOpt, colorOpt, visibleOpt, valueOpt] {
            if (nameOpt->count() == 0 && colorOpt->count() == 0
                && visibleOpt->count() == 0 && valueOpt->count() == 0) {
                throw std::runtime_error(
                    "update needs at least one of --name, --color, --visible/--hidden, --value");
            }
            auto idBytes = sysex::encode14bit(static_cast<uint16_t>(id));
            std::vector<uint8_t> params{ idBytes[0], idBytes[1] };

            JsonDocument doc;
            if (nameOpt->count() > 0)
                doc["name"] = name;
            if (colorOpt->count() > 0)
                doc["color"] = color;
            if (visibleOpt->count() > 0)
                doc["visible"] = visible;
            if (valueOpt->count() > 0)
                doc["value"] = value;
            auto jsonBytes = commands::jsonToBytes(doc);
            params.insert(params.end(), jsonBytes.begin(), jsonBytes.end());

            runner::runAction(ctx, 0x14, 0x07, params);
        });
    }
    {
        static int id = 0;
        static int valueId = 0;
        static std::string text;
        auto *sub = control->add_subcommand(
            "override-text",
            "Override Value Text (runtime, max 15 characters)");
        sub->add_option("--id", id, "Control ID")->required();
        sub->add_option("--value-id", valueId, "Value ID (0x00-0x08)")
            ->required();
        sub->add_option("--text", text, "Override text")->required();
        sub->callback([&ctx] {
            if (text.size() > 15) {
                throw std::runtime_error(
                    "override text must be at most 15 characters");
            }
            auto idBytes = sysex::encode14bit(static_cast<uint16_t>(id));
            std::vector<uint8_t> params{ idBytes[0],
                                         idBytes[1],
                                         static_cast<uint8_t>(valueId) };
            auto textBytes = sysex::encodeAscii(text);
            params.insert(params.end(), textBytes.begin(), textBytes.end());
            runner::runAction(ctx, 0x14, 0x0E, params);
        });
    }
}
