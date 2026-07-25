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

#include "commands/progress.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace commands
{

    std::string formatProgressBar(size_t sent, size_t total, int barWidth)
    {
        if (sent > total)
            sent = total;
        double fraction =
            total > 0 ? static_cast<double>(sent) / static_cast<double>(total)
                      : 1.0;

        int filled = static_cast<int>(fraction * barWidth + 0.5);
        filled = std::max(0, std::min(barWidth, filled));
        int percent = static_cast<int>(fraction * 100.0 + 0.5);
        percent = std::max(0, std::min(100, percent));

        std::string totalStr = std::to_string(total);

        std::ostringstream os;
        os << '[';
        for (int i = 0; i < barWidth; ++i) {
            if (i < filled)
                os << '=';
            else if (i == filled)
                os << '>';
            else
                os << ' ';
        }
        os << "] " << std::setw(3) << percent << "% "
           << std::setw(static_cast<int>(totalStr.size())) << sent << "/"
           << totalStr << " bytes";
        return os.str();
    }

} // namespace commands
