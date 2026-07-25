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

#include <cstddef>

namespace commands
{

    // True if stdout is attached to a real terminal rather than redirected or
    // piped. Used to gate anything that updates a line in place (a progress
    // bar, the `--human` grid layout for arrays of scalars) - that only makes
    // sense on an actual terminal.
    bool isStdoutTerminal();

    // Current terminal width in columns, or 80 if it can't be determined (not a
    // terminal, or the platform call failed).
    size_t terminalWidth();

} // namespace commands
