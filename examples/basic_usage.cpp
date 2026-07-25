// Minimal example of driving an Electra One from C++ via libelectraone.
//
// Build (from the repo root, works identically on macOS/Linux/Windows):
//   cmake -B build && cmake --build build
//   ./build/basic_usage        (Windows: build\Release\basic_usage.exe or similar)
//
// macOS/Linux only, as a quick single-file alternative once the library is
// built - see examples/Makefile:
//   cd examples && make && ./basic_usage
//
// See README.md for full per-platform prerequisites.

#include <electraone/ElectraOneClient.hpp>
#include <iostream>

int main()
{
    electraone::Client client;
    try {
        client
            .connect(); // finds the CTRL port by name; see ConnectOptions to override
    } catch (const std::exception &e) {
        std::cerr << "connect failed: " << e.what() << "\n";
        return 1;
    }

    auto info = client.getElectraInfo();
    if (!info.has_value()) {
        std::cerr << "timed out waiting for a reply\n";
        return 1;
    }
    if (info->isNack) {
        std::cerr << "device NACKed the request\n";
        return 1;
    }
    std::cout << "Electra info: " << info->payloadAsText() << "\n";

    // Runtime commands return a plain ACK/NACK rather than data.
    auto pageSwitch = client.switchPage(0);
    std::cout << "switch to page 0: "
              << (pageSwitch && pageSwitch->isAck ? "OK" : "failed") << "\n";

    // File upload example (commented out - uncomment and point at a real
    // file/destination to try it):
    //
    // electraone::UploadFileOptions opts;
    // opts.location = "slots";
    // opts.type = "luaModule";
    // opts.ns = "mymodule";
    // opts.path = "init";
    // opts.onProgress = [](size_t sent, size_t total) {
    //     std::cout << "uploaded " << sent << "/" << total << " bytes\n";
    // };
    // std::vector<uint8_t> content = {'p', 'r', 'i', 'n', 't', '(', '1', ')'};
    // auto commit = client.uploadFile(content, opts);  // throws on failure

    return 0;
}
