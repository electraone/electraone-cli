# electraone

A cross-platform command-line suite *and* C++ client library for the Electra One SysEx protocol — [core implementation](https://docs.electra.one/developers/midiimplementation.html) and [File Transfer API](https://docs.electra.one/developers/filetransfer.html) — built on RtMidi. Every documented SysEx command is exposed both as a CLI subcommand and as a C++ method, talking to the Electra One's **CTRL** MIDI port.

## Building

Requires CMake 3.16+ and a C++17 compiler. Dependencies (RtMidi, CLI11, ArduinoJson) are picked up via `find_package` if already installed, otherwise fetched from source and built (as a static library, so the results below are self-contained - no `.dll`/`.so`/`.dylib` to keep alongside the binaries) automatically at configure time. The same two commands build everything - the CLI, the C++ library, and the example - on macOS, Linux, and Windows:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This produces `build/electraone` (the CLI), `build/libelectraone.a` (the C++ client library - see [C++ API library](#c-api-library) below), and `build/basic_usage` (the example - set `-DELECTRAONE_BUILD_EXAMPLES=OFF` to skip it). Exact output paths/names differ slightly per platform (see below).

### Prerequisites per platform

**macOS** — Xcode Command Line Tools (`xcode-select --install`) for a C++17 compiler; CMake (`brew install cmake`, or the [cmake.org](https://cmake.org/download/) installer); Git (for `FetchContent` to clone dependencies at configure time, comes with Xcode CLT). RtMidi builds against **CoreMIDI/CoreAudio/CoreFoundation**, already part of macOS - nothing else to install. This is the platform this project has actually been built and run against (including live against real hardware); Linux and Windows below are supported by design and code review, not yet build-verified here.

**Linux** — a C++17 compiler and CMake (Debian/Ubuntu: `sudo apt install build-essential cmake git`; Fedora: `sudo dnf install gcc-c++ cmake git`). RtMidi's Linux backend needs **ALSA development headers** to build a working MIDI backend: `sudo apt install libasound2-dev` (Debian/Ubuntu) or `sudo dnf install alsa-lib-devel` (Fedora/RHEL). Without these, CMake's own configure step will print `Could NOT find ALSA` and RtMidi will build without a real MIDI backend - install them *before* running `cmake -B build` (delete `build/` and reconfigure if you installed them afterward, since FetchContent's RtMidi detection runs once at configure time).

**Windows** — Visual Studio 2022 (the free Community edition works) with the **"Desktop development with C++"** workload, which provides MSVC, the Windows SDK (so RtMidi's **WinMM** backend is available with nothing extra to install), and CMake integration. Also install Git if the VS installer didn't already put it on `PATH` (needed for `FetchContent`). Build from a "Developer Command Prompt for VS 2022" or via VS's built-in CMake support:

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

(Visual Studio is a multi-config generator, so binaries land in `build\Release\` rather than directly in `build\`, e.g. `build\Release\electraone.exe`.) If you'd rather use MinGW-w64/GCC instead of MSVC, that should also work (RtMidi supports it) via `cmake -B build -G "MinGW Makefiles"` from an MSYS2/MinGW shell, but MSVC is the better-tested path for this project.

Every platform's example (`basic_usage`) builds automatically via the commands above - no manual linking needed on any of them. macOS/Linux also have [examples/Makefile](examples/Makefile) as an optional quick single-file rebuild once the library exists; it's not used on Windows (see the comment at its top).

## Testing

`cmake --build build` also builds `electraone_tests` (skip with `-DELECTRAONE_BUILD_TESTS=OFF`), covering the `electraone_api` library's hardware-independent logic with [doctest](https://github.com/doctest/doctest): the SysEx envelope encode/decode in [sysex.cpp](include/electraone/sysex.hpp) (`encode14bit`/`encode28bit`/`encodeAscii`/`buildMessage`/`parseResponse`, including malformed input and the transaction-id framing), the [MD5](src/md5.hpp) implementation `uploadFile`'s Commit step relies on (checked against RFC 1321 test vectors), and `electraone::describeEvent` (every event type, including the two documented byte-collisions noted below). `Client`'s command methods themselves aren't unit tested, since they need a live MIDI connection rather than being pure logic - that's what [examples/basic_usage.cpp](examples/basic_usage.cpp) exercises by hand against real hardware.

```bash
ctest --test-dir build --output-on-failure
```

or run `./build/electraone_tests` directly for doctest's own output (`--help` for its filtering/reporting options).

[.github/workflows/ci.yml](.github/workflows/ci.yml) runs the same configure/build/test sequence on GitHub Actions across `ubuntu-latest`, `macos-latest`, and `windows-latest` on every push and pull request - the tests don't touch real hardware, so this works unattended. As with the rest of this README, macOS is the platform this has actually been run on locally; Linux and Windows are exercised by CI itself once pushed, which is the point of having it there.

## Connecting

```bash
electraone list-ports
```

lists all MIDI input/output ports. By default every command connects to the port whose name contains `CTRL` (case-insensitive) — on most systems that's `Electra Controller Electra CTRL`, found automatically with no configuration. Override with:

- `--port <substring>` — match a different substring
- `--port-index <n>` — use a specific port index from `list-ports` directly, bypassing name matching

Other global options (must be given before the subcommand name):

- `--timeout <ms>` — reply wait timeout, default 3000
- `--txn-id <n>` — attach an optional 14-bit transaction ID to the request (firmware 4.0+)
- `-o, --output <file>` — write the response to a file instead of stdout
- `--raw` — print the raw response bytes as hex instead of decoding
- `--pretty` — pretty-print JSON responses with indentation for human reading (non-JSON payloads, e.g. Lua source, are printed unchanged; has no effect together with `--raw`)
- `--human` — render JSON responses for human reading rather than as JSON at all: objects print as an indented tree that preserves the JSON's own key/value structure, and arrays (top-level or nested under a key, e.g. `preset list`'s `presets` field) print `ls`-style - a column-aligned table when the elements are objects, a terminal-width-aware multi-column grid when they're scalars (falling back to one-per-line when stdout isn't a terminal, like `ls` piped to a file). Non-JSON payloads are printed unchanged. Takes precedence over `--pretty`; no `-h` short form since that's already `--help` on every subcommand.

Exit codes: `0` success/ACK, `1` NACK, `2` timeout or transport/protocol error, `3` CLI usage error.

## Command groups

Run `electraone <group> --help` or `electraone <group> <subcommand> --help` for full details.

| Group | Subcommands |
|---|---|
| (top-level) | `list-ports`, `info`, `runtime-info`, `reboot`, `debug enable\|disable`, `midi-learn enable\|disable\|listen`, `usb-devices list` |
| `preset` | `get`, `upload <file>`, `remove`, `clear-slot`, `list`, `slot-info`, `switch`, `set-slot`, `reload`, `load-preloaded` |
| `lua` | `get`, `upload <file>`, `remove`, `exec <code>\|--file` |
| `overrides` | `get`, `upload <file>` |
| `persisted` | `get`, `upload <file>` |
| `performance` | `get`, `upload <file>` |
| `config` | `get`, `upload <file>`, `remove` |
| `snapshot` | `list`, `get`, `update`, `remove`, `swap`, `set-slot` |
| `capture` | `list`, `get`, `update`, `remove`, `swap`, `set-slot` (see ambiguity note below) |
| `control` | `update`, `override-text` |
| `ui` | `page-switch`, `control-set-switch`, `bottom-bar-text` |
| `events` | `set-port`, `subscribe`, `listen` |
| `logger` | `enable`, `disable`, `set-port`, `listen` |
| `window` | `stop`, `resume` |
| `files` | `open`, `register`, `send-chunk`, `commit`, `list`, `remove`, `upload <file>` (composite) — see below |
| `raw` | escape hatch: send an arbitrary category/command/payload or a literal hex byte string |

### Examples

```bash
electraone info
electraone --pretty preset list
electraone --human preset list
electraone preset get --bank 0 --slot 1
electraone preset upload my-preset.json
electraone snapshot list --project-id abc123
electraone lua exec "print('hello')"
electraone control update --id 42 --value 100
electraone events listen --pots --touch --duration 30
electraone raw --category 0x02 --command 0x7F
```

File arguments accept `-` to read from stdin, e.g. `cat preset.json | electraone preset upload -`.

## File Transfer API (`files`)

The [File Transfer API](https://docs.electra.one/developers/filetransfer.html) (firmware 4.0+) uploads/lists/removes files stored on the device, using an atomic transaction: open a cache, register one or more files with their size, stream each one in chunks, then commit with an MD5 checksum per file — the device verifies every checksum and either applies all files or rejects the whole transaction.

For the common case of uploading a single file, use the composite command — it runs the whole transaction for you:

```bash
electraone files upload my-script.lua --location slots --type luaModule --namespace mymodule --path init
electraone files upload my-preset.json --location slots --type preset --bank 0 --slot 1
```

`--location` is one of `slots`, `updates`, `assets`, `modules`, `presets`, `root`; `--type` is one of `firmware`, `bootloader`, `preset`, `lua`, `luaModule`, `ui`, `config`, `deviceList`, `datafile`, `performance`. Pass `--bank`/`--slot` for `slots` locations, or `--namespace`/`--path` for `modules`/`presets` locations, per the destination's requirements. `--id` (default 1) only needs to be unique within one transaction; `--chunk-size` (default 256 bytes) controls how much file data goes in each Transfer Chunks message.

Commit validates every file's MD5 on-device before applying anything, which can take much longer than any other command for a large file - `--commit-timeout` (default 60000ms) covers just that final step, independent of the general `--timeout` (which still applies to opening, registering, and each chunk, so those still fail fast if something's actually wrong). Bump `--commit-timeout` further if a commit still times out on a very large file. The low-level `files commit` subcommand takes the same option.

The low-level primitives (`files open`, `files register`, `files send-chunk`, `files commit`, `files list`, `files remove`) are also exposed individually, e.g. for scripting a multi-file transaction (`open` once, `register` + `send-chunk` per file, one `commit` with a hand-written JSON `files[]` array covering all of them) or for debugging.

**Chunk encoding caveat**: the docs describe Transfer Chunks' payload only as "MIDI 7-bit encoded," without specifying a bit-packing algorithm for arbitrary 8-bit bytes. Consistent with every other payload in the protocol (which is literal ASCII/7-bit, no packing), `files upload`/`files send-chunk` send each source byte as-is and reject the file up front if any byte is ≥ 0x80. That covers the realistic cases — Lua scripts, JSON presets/config/deviceList, datafiles — since they're text already. For genuinely binary content (`firmware`/`bootloader`), pass `--allow-binary` to bypass that check; bytes are still sent unpacked, so on real binary data this will most likely fail with an MD5 mismatch at commit rather than corrupting anything (commit is atomic and verifies MD5 before applying), but there's no verified packing scheme to fall back to. If you find the real one, it belongs in `sendChunk`/`upload` in [files_commands.cpp](src/commands/files_commands.cpp).

## C++ API library

`electraone_api` builds `libelectraone.a` plus a public header at `include/electraone/` (`ElectraOneClient.hpp` and `sysex.hpp`) — a C++ client for the same protocol the CLI implements, for embedding directly in a 3rd-party application instead of shelling out to the CLI. It has no CLI11 dependency; its public header only needs the STL. RtMidi is still required at link time (see below), but nothing about it leaks into the public API.

```cpp
#include <electraone/ElectraOneClient.hpp>

electraone::Client client;
client.connect();                                   // throws std::runtime_error on failure

auto info = client.getElectraInfo();
if (!info)               { /* timed out */ }
else if (info->isNack)   { /* device rejected the request */ }
else                      std::cout << info->payloadAsText();  // JSON text

client.switchPage(2);

electraone::UploadFileOptions opts;
opts.location = "slots";
opts.type = "preset";
opts.bank = 0;
opts.slot = 1;
client.uploadFile(fileBytes, opts);  // throws on failure
```

Every command method returns `std::optional<Response>` (`Response` is an alias for `sysex::ParsedResponse`): `std::nullopt` means the device didn't reply within the timeout; otherwise check `.isNack`/`.isAck` and `.payloadAsText()`/`.payload`. `Client::send(category, command, params)` is the same low-level escape hatch as the CLI's `raw` command. `Client::poll(timeoutMs)` waits for the next incoming message without sending anything, for consuming unsolicited events after `subscribeEvents`/`enableLogger` — pair it with the free function `electraone::describeEvent(response)` for a human-readable rendering. `uploadFile` runs the whole open+register+chunk+commit File Transfer transaction and throws `std::runtime_error` naming the failed step; the same `--allow-binary` caveat from the CLI applies (see `UploadFileOptions::allowBinary`). Commit validates MD5 on-device and can take much longer than any other command for a large file, so it gets its own generous default timeout independent of `ConnectOptions::timeoutMs` - see `UploadFileOptions::commitTimeoutMs` (default 60s) and `Client::commitTransaction`'s own `timeoutMs` parameter if calling it directly. The full method list mirrors the [command groups table](#command-groups) above — see [ElectraOneClient.hpp](include/electraone/ElectraOneClient.hpp) for exact signatures, and [examples/basic_usage.cpp](examples/basic_usage.cpp) for a runnable example, buildable with its own [Makefile](examples/Makefile):

```bash
cmake -B build && cmake --build build   # builds libelectraone.a
cd examples && make && ./basic_usage
```

**Linking**: from another CMake project, the simplest path is `add_subdirectory` (or `FetchContent`) this repo and `target_link_libraries(your_app PRIVATE electraone_api)` — RtMidi (statically linked, see [Prerequisites per platform](#prerequisites-per-platform) above) and its system MIDI backend (CoreMIDI/ALSA/WinMM) come along transitively, no further configuration needed. This is also how `basic_usage` itself is built (see its `add_executable`/`target_link_libraries` calls in [CMakeLists.txt](CMakeLists.txt)) - copy that pattern for your own app.

Without CMake, link `libelectraone.a` plus RtMidi's static archive plus the platform's MIDI libraries directly:

```bash
# macOS
c++ -std=c++17 -I include your_app.cpp build/libelectraone.a build/_deps/rtmidi-build/librtmidi.a \
  -framework CoreMIDI -framework CoreAudio -framework CoreFoundation -o your_app

# Linux
c++ -std=c++17 -I include your_app.cpp build/libelectraone.a build/_deps/rtmidi-build/librtmidi.a \
  -lasound -lpthread -o your_app
```

```
:: Windows (Developer Command Prompt for VS; adjust the Release\ paths if
:: you used a single-config generator like Ninja, where there's no config
:: subfolder)
cl /std:c++17 /I include your_app.cpp build\Release\electraone.lib build\_deps\rtmidi-build\Release\rtmidi.lib ^
  winmm.lib /Fe:your_app.exe
```

(If RtMidi came from `find_package` instead of being fetched - i.e. you pre-installed it system-wide - swap the `librtmidi.a`/`rtmidi.lib` path for a plain `-lrtmidi`/`rtmidi.lib`, and note it may be a shared library in that case, defeating the self-contained-binary property described above; that's outside this project's control since it means using whatever the system package provides.) `cmake --install build` installs the archive to `lib/` and both public headers to `include/electraone/` for system-wide use on any platform.

The manual Windows command above is a best-effort translation of the macOS/Linux ones based on how MSVC and CMake's Visual Studio generator normally lay things out - it hasn't actually been run on a Windows machine. The CMake `target_link_libraries(your_app PRIVATE electraone_api)` approach just above is the one to trust; it doesn't depend on guessing paths like this.

## Known documentation ambiguities

The upstream docs page has a couple of apparent copy/paste errors that this tool works around but can't fully resolve on its own:

- **Capture vs. snapshot byte collision**: Remove/Update/Swap Capture are documented with the *exact same* category/command bytes as Remove/Update/Swap Snapshot (`0x05 0x06`, `0x04 0x06`, `0x06 0x06`), and neither JSON payload has a field to disambiguate. This is almost certainly a docs error — the device can't tell them apart as documented. `capture update|remove|swap` send the documented bytes, but treat them as unverified. If you find the real values (firmware source, Electra support, or packet-sniffing the official editor), override them with `electraone raw --category ... --command ...` rather than filing this as a bug.
- **Preset Bank Switch vs. USB Host Change**: both documented as `0x7E 0x08`, differing only by whether a bank byte is present. `events listen`/`logger listen` disambiguate by payload length (0 bytes = USB Host Change, 1 byte = Preset Bank Switch), which works but isn't a documented guarantee.
- **Capture Data payload encoding**: documented only as "raw MIDI capture data," unlike every other payload (which is explicit ASCII/JSON). `capture get` decodes as text by default; pass `--raw` to dump the untouched response bytes if that turns out to be wrong.

## Troubleshooting a timeout

If a command times out waiting for a reply (exit code 2) but `list-ports` shows the CTRL port:

- Check the Electra One's **Settings → MIDI Control** is enabled — the CTRL port only processes SysEx commands when this is on.
- Make sure no other app (e.g. the Electra One Editor / Preset Librarian) is already connected to the CTRL port — most MIDI backends don't allow two exclusive listeners.
- Confirm the device is awake and not mid-update.
- Try `electraone --timeout 8000 info` in case the device is just slow to respond.
