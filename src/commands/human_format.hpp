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
 * @file human_format.hpp
 *
 * @brief Renders a JSON response as human-friendly output for `--human`:
 * objects as an indented tree, arrays of objects as an `ls -l`-style table,
 * and arrays of scalars as an `ls`-style grid.
 */

#pragma once

#include <string>

namespace commands
{
    /**
     * @brief Renders JSON text for human reading.
     * @param text Response text.
     * @return The rendered text, or text unchanged if it isn't valid JSON.
     */
    std::string formatHumanReadable(const std::string &text);

} // namespace commands
