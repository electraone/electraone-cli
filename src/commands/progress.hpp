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
#include <string>

namespace commands
{

    // Renders a single fixed-width progress bar line, e.g.:
    //   [=============>               ]  52% 4200/8054 bytes
    // `sent` is clamped to `total` and total == 0 renders as complete (nothing
    // to send). The numeric fields are padded to `total`'s own width, so the
    // line's length never changes between updates - the caller can redraw it
    // in place with a leading '\r' without leftover characters from a
    // previous, longer line.
    std::string formatProgressBar(size_t sent, size_t total, int barWidth = 30);

} // namespace commands
