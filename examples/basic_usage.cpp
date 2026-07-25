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

/*
 * Minimal example of driving an Electra One from C++ via libelectraone.
 *
 * Build (from the repo root, works identically on macOS/Linux/Windows):
 *   cmake -B build && cmake --build build
 *   ./build/basic_usage     (Windows: build\Release\basic_usage.exe or similar)
 *
 * macOS/Linux only, as a quick single-file alternative once the library is
 * built - see examples/Makefile:
 *   cd examples && make && ./basic_usage
 *
 * See README.md for full per-platform prerequisites.
 */

#include <electraone/ElectraOneClient.hpp>
#include <iostream>

int main()
{
    electraone::Client client;

    /*
     * finds the CTRL port by name; see ConnectOptions to override
     */
    try {
        client.connect();
    } catch (const std::exception &e) {
        std::cerr << "connect failed: " << e.what() << "\n";
        return 1;
    }

    /*
     * Get the info about connected Electra One controller
     */
    auto info = client.getElectraInfo();
    if (!info.has_value()) {
        std::cerr << "timed out waiting for a reply\n";
        return 1;
    }

    std::cout << "Electra info: " << info->payloadAsText() << "\n";

    /*
     * Runtime commands return a plain ACK/NACK rather than data.
     */
    auto pageSwitch = client.switchPage(0);
    std::cout << "switch to page 0: "
              << (pageSwitch && pageSwitch->isAck ? "OK" : "failed") << "\n";

    return 0;
}