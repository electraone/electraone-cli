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
 * @file progress.hpp
 *
 * @brief Progress bar rendering for `files upload`'s chunk-sending loop.
 */

#pragma once

#include <cstddef>
#include <string>

namespace commands
{
    /**
     * @brief Renders a single-line progress bar, e.g. "[==>    ] 40%".
     * @param sent Bytes sent so far.
     * @param total Total bytes to send; 0 renders as complete.
     * @param barWidth Width of the bar itself, in characters.
     * @return The rendered line (no trailing newline).
     */
    std::string formatProgressBar(size_t sent, size_t total, int barWidth = 30);

} // namespace commands
