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

#include <string>

namespace commands
{

    // Renders JSON text for human reading: objects are printed as an indented
    // tree that preserves the JSON's own key/value structure; arrays are
    // printed `ls`-style - a column-aligned table when the elements are
    // objects, a terminal-width-aware multi-column grid when they're scalars.
    // If text doesn't parse as JSON, it's returned unchanged.
    std::string formatHumanReadable(const std::string &text);

} // namespace commands
