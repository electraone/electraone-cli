# electraone-cli

A cross-platform command-line suite and C++ client library for the Electra One
SysEx protocol:

- [Electra Core SysEx implementation](https://docs.electra.one/developers/midiimplementation.html)
- [File Transfer API](https://docs.electra.one/developers/filetransfer.html)

The library and command-line tool provide programmatic access to all Electra One
SysEx commands. They can be used to manage the controller from the command line
and to develop software applications that communicate with it without having to
implement the underlying MIDI and SysEx communication details.

A small demo application is included to demonstrate how to use the C++ client
library.

## Building

In order to build the application and the library the following dependencies
are required:

- CMake 3.16+
- C++17 compiler
- RtMidi
- CLI11
- ArduinoJson

RtMidi, CLI11, and ArduinoJson are picked up via `find_package` if already
installed, otherwise fetched from source and built (as a static library)

The binaries can be build on all platforms (Lunix, MacOs, Windows) using:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This produces:

- `build/electraone` - the command line application (CLI)
- `build/libelectraone.a` - the C++ client library, see [C++ API library](#c-api-library) below
- `build/basic_usage` - the example - set `-DELECTRAONE_BUILD_EXAMPLES=OFF` to skip it

Exact output paths / names differ slightly per platform.

### Prerequisites per platform

#### Linux

A C++17 compiler and CMake are required. RtMidi's Linux backend needs
ALSA development headers to build a working MIDI backend:

`sudo apt install libasound2-dev` (Debian/Ubuntu) or
`sudo dnf install alsa-lib-devel` (Fedora/RHEL).

Without these, CMake's own configure step will print `Could NOT find ALSA` and
RtMidi will build without a real MIDI backend. Install them before running
`cmake -B build`. Delete `build/` and reconfigure if you installed them
afterward, since FetchContent's RtMidi detection runs once at configure time.

#### macOS

Xcode Command Line Tools (`xcode-select --install`) for a C++17 compiler,
CMake (`brew install cmake`, or the [cmake.org](https://cmake.org/download/)
installer). RtMidi builds against CoreMIDI/CoreAudio/CoreFoundation, which
already is part of macOS.

#### Windows

Visual Studio 2022 (the free Community edition works) with the
"Desktop development with C++", which provides MSVC, the Windows
SDK, and CMake integration. Also install and configure Git if the VS installer
didn't already put it on `PATH`.

Build from a "Developer Command Prompt for VS" or via VS's built-in CMake
support:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Testing

`cmake --build build` also builds `electraone_tests`
(it can be skipped with `-DELECTRAONE_BUILD_TESTS=OFF`), covering the
`electraone_api` library's hardware-independent logic with
[doctest](https://github.com/doctest/doctest).

To run the tests issue:

```bash
ctest --test-dir build --output-on-failure
```

or run `./build/electraone_tests` directly for doctest's own output (`--help`
for its filtering/reporting options).

## Connecting

Once the command line tool is built, you can use it to interact wih the
Electra One hardware controller. The first step is to review the MIDI ports:

```bash
electraone list-ports
```

The command lists all MIDI input/output ports available on the host computer.
By default, every command connects to the port whose name contains `CTRL`
(case-insensitive). In general, this will work on Linux and MacOs.

If the default `CTRL` port does not work, specify the Electra One control port
using the command line parameters:

- `--port <substring>` to match a different substring
- `--port-index <n>` to use a specific port index (same one for both output and
  input) from `list-ports` directly, bypassing name matching
- `--out-port-index <n>` / `--in-port-index <n>` for situations when MIDI input
  and output have different index number, see bellow.

**Windows note**: WinMM's port names may not contain `CTRL` (they're generic,
e.g. `MIDIOUT3 (Electra Controller)`), so name matching always fails there and
`--port-index` (or the pair above) is required. Windows also commonly lists an
extra software-only output port (e.g. `Microsoft GS Wavetable Synth`) ahead of
any real devices, with no matching input port. That shifts every subsequent
output index one higher than the same device's input index. Run `list-ports`
and compare: if the Electra's output and input entries land at different
positions, use `--out-port-index`/`--in-port-index` instead of `--port-index`
to address them independently.

## Global options

Besides the options to specify connection ports, there is a handful of other
global options. The global options must be given before the subcommand name.
They are:

- `--timeout <ms>` — reply wait timeout, default 3000
- `--txn-id <n>` — attach an optional 14-bit transaction ID to the request
  (firmware 4.0+)
- `-o, --output <file>` — write the response to a file instead of stdout
- `--raw` — print the raw response bytes as hex instead of decoding
- `--pretty` — pretty-print JSON responses with indentation for human reading
  (non-JSON payloads, e.g. Lua source, are printed unchanged; has no effect
  together with `--raw`)
- `--human` — render JSON responses for human reading rather than as JSON.
  It akes precedence over `--pretty`; no `-h` short form since that's already
  `--help` on every subcommand.

## Exit codes

The command line tool completes the its task it returns an exit code depending
on the result of the task.

Exit codes:

- `0` ACK (success)
- `1` NACK (failure)
- `2` timeout or transport/protocol error
- `3` CLI usage error

## Transactions

Any command that changes the state or data on the controller can be associated
with a transaction ID. The primary purpose of transaction IDs is to link an
ACK/NACK response to the original request.

Transaction IDs are 14-bit numbers. The host is responsible for generating them
and ensuring their uniqueness.

Data requests, such as list and information queries, do not support transaction
IDs.

## Command groups

Run `electraone <group> --help` or `electraone <group> <subcommand> --help` for
full details about groups and their sub-commands.

| Group           | Subcommands                                                                                                           |
| --------------- | --------------------------------------------------------------------------------------------------------------------- |
| `list-ports`    |                                                                                                                       |
| `info`          |                                                                                                                       |
| `runtime-info`  |                                                                                                                       |
| `reboot`        |                                                                                                                       |
| `preset`        | `get`, `upload <file>`, `remove`, `clear-slot`, `list`, `slot-info`, `switch`, `set-slot`, `reload`, `load-preloaded` |
| `lua`           | `get`, `upload <file>`, `remove`, `exec <code>\|--file`                                                               |
| `overrides`     | `get`, `upload <file>`                                                                                                |
| `persisted`     | `get`, `upload <file>`                                                                                                |
| `performance`   | `get`, `upload <file>`                                                                                                |
| `config`        | `get`, `upload <file>`, `remove`                                                                                      |
| `snapshot`      | `list`, `get`, `update`, `remove`, `swap`, `set-slot`                                                                 |
| `capture`       | `list`, `get`, `update`, `remove`, `swap`, `set-slot`                                                                 |
| `control`       | `update`, `override-text`                                                                                             |
| `parameter-map` | `list`                                                                                                                |
| `midi-learn`    | `enable`, `disable`, `listen`                                                                                         |
| `usb-devices`   | `list`                                                                                                                |
| `ui`            | `page-switch`, `control-set-switch`, `bottom-bar-text`                                                                |
| `events`        | `set-port`, `subscribe`, `listen`                                                                                     |
| `logger`        | `enable`, `disable`, `set-port`, `listen`                                                                             |
| `window`        | `stop`, `resume`                                                                                                      |
| `debug`         | `enable`, `disable`, `set-breakpoints`                                                                                |
| `files`         | `open`, `register`, `send-chunk`, `commit`, `list`, `remove`, `upload <file>` (composite) — see below                 |
| `screenshot`    |                                                                                                                       |

### Examples

```bash
electraone info
electraone --pretty preset list
electraone --human preset list
electraone preset get --bank 0 --slot 1
electraone preset upload my-preset.json
electraone snapshot list --project-id nljaziUjglOuD1fe15Eq
electraone lua exec "print('hello')"
electraone control update --id 42 --value-text "hello"
electraone events listen --pots --touch --duration 30
```

File arguments accept `-` to read from stdin,
e.g. `cat preset.json | electraone preset upload -`.

### Listening

The command line not only sends commands to the Electra One controller. It can
also listen to events and log messages that the controller emits.

The commands to listen are:

- `events listen`
- `logger listen`
- `midi-learn listen`

All listen commands print incoming messages as `[HH:MM:SS.mmm] <description>`,
where the timestamp is a host-side wall-clock timestamp on every line and
the `<description>` is the message or event sent by the controller.

`logger listen` subscribes to controller events in addition to enabling the
logger (same `--page` `--control-set` `--usb-host` `--pots` `--touch`
`--button` `--window` `--all` flags as `events listen`, defaulting to all event
types), so you see Pot Touch / Page Switch / etc. interleaved with log messages
in one stream instead of having to run two terminals:

```bash
electraone logger listen --duration 30
electraone logger listen --pots --touch  # log messages + just these event types
```

`--timeout` option does not apply to `listen` commands. `listen` is inherently
open-ended. With no `--duration` option specified, it waits indefinitely
until Ctrl+C, and with `--duration N` it gives up once N seconds have passed.

## File Transfer API (`files`)

The [File Transfer API](https://docs.electra.one/developers/filetransfer.html),
available in firmware 4.0 and later, supports uploading, listing, and removing
files stored on the controller.

File uploads use an atomic transaction:

1. Open a cache.
2. Register one or more files and specify their sizes.
3. Transfer each file in chunks.
4. Commit the transaction with an MD5 checksum for each file.

During the commit step, the controller verifies the checksums of all transferred
files. It then either applies every file in the transaction or rejects the entire
transaction.

### Uploading a single file

For the common case of uploading a single file, use the composite `upload`
command. It performs the entire transaction automatically:

```bash
electraone files upload firmware-mk2-v4.1.4.srec \
  --location updates \
  --type firmware \
  --chunk-size 32768

electraone files upload my-preset.json \
  --location slots \
  --type preset \
  --bank 0 \
  --slot 1
```

### File locations

The `--location` option specifies where the file will be stored:

- `slots` — preset slots that users can navigate from the controller UI
- `updates` — staging area for updates applied during the next reboot
- `assets` — UI assets, fonts, and related files
- `modules` — preloaded Lua modules
- `presets` — preloaded presets

### File types

The `--type` option specifies the type of file being uploaded:

- `firmware` — Electra One firmware
- `bootloader` — Electra One bootloader; use this type with caution
- `preset` — preset JSON file
- `lua` — preset Lua source file
- `luaModule` — shared Lua module that can be used by preset Lua scripts
- `ui` — UI asset file
- `config` — controller configuration JSON file
- `deviceList` — JSON file containing device override definitions
- `datafile` — JSON file containing persistent data generated by a Lua script
- `performance` — performance JSON file

### Destination addressing

Different locations use different destination addressing schemes:

- Preset `slots` are identified by `--bank` and `--slot`, using 0-based index.
- Preloaded `modules` and `presets` are identified by `--namespace` and
  `--path`. The namespace is the top-level directory, typically named after the
  maintainer. The path identifies a file or subdirectory within that namespace.

### Multi-file transactions

When transferring multiple files in a single transaction, each file must be
assigned an identifier with `--id`.

The identifier defaults to `1` and only needs to be unique within the current
transaction.

Files are transferred in smaller chunks. Use `--chunk-size` to control the
amount of file data included in each transfer-chunk message. The default chunk
size is 256 bytes.

### Upload progress

While sending chunks, `files upload` shows a progress bar that updates in
place:

```
[================>             ]  52% 14000/26682 bytes
```

The bar only appears when stdout is an actual terminal - if the output is
redirected or piped, nothing is printed per chunk (the MD5/step/summary lines
around the transfer are still shown either way). Pass `--no-progress` to
suppress the bar entirely, even on a terminal.

### Commit timeout

During the commit step, the controller validates the MD5 checksum of every file
before applying any changes. For large files, this step can take significantly
longer than other commands.

The `--commit-timeout` option controls the timeout for the commit step only. Its
default value is 60,000 ms, and it is independent of the general `--timeout`
option.

### Low-level commands

The following low-level commands are available:

- `files open`
- `files register`
- `files send-chunk`
- `files commit`
- `files list`
- `files remove`

are exposed individually, e.g. for scripting a multi-file transaction.

## C++ API library

The `electraone-cli` repository also builds `libelectraone.a`, a static C++
library that provides programmatic access to the same Electra One SysEx protocol
used by the CLI.

The public headers are located in `include/electraone/`:

- `ElectraOneClient.hpp` — high-level client API
- `sysex.hpp` — lower-level SysEx types and utilities

The library is intended for embedding Electra One support directly into
third-party applications.

The public API depends only on the C++ Standard Library. RtMidi is required when
linking the application, but RtMidi types and implementation details are not
exposed through the public API.

### Basic usage

```cpp
#include <electraone/ElectraOneClient.hpp>

electraone::Client client;
client.connect();  // Throws std::runtime_error on failure.

auto info = client.getElectraInfo();

if (!info) {
    // The controller did not reply before the timeout.
} else if (info->isNack) {
    // The controller rejected the request.
} else {
    std::cout << info->payloadAsText();  // JSON response.
}

client.switchPage(2);

electraone::UploadFileOptions options;
options.location = "slots";
options.type = "preset";
options.bank = 0;
options.slot = 1;

client.uploadFile(fileBytes, options);  // Throws on failure.
```

### Responses and timeouts

Command methods return `std::optional<Response>`, where `Response` is an alias
for `sysex::ParsedResponse`.

The return value should be interpreted as follows:

- `std::nullopt` — the controller did not reply before the timeout
- `response.isAck` — the controller accepted the request
- `response.isNack` — the controller rejected the request
- `response.payloadAsText()` — returns the response payload as text
- `response.payload` — provides access to the raw payload bytes

### Receiving unsolicited events

Use `Client::poll(timeoutMs)` to wait for the next incoming message without
sending a request.

This is useful after enabling event streams with methods such as
`subscribeEvents()` or `enableLogger()`.

The function describeEvent

```cpp
electraone::describeEvent(response)
```

converts an event response into a human-readable description.

### Uploading files

`Client::uploadFile()` performs the complete File Transfer API transaction:

1. Open the transfer cache.
2. Register the file.
3. Send the file in chunks.
4. Commit the transaction.

If any step fails, the method throws `std::runtime_error`. The exception message
identifies the failed step.

During the commit step, the controller validates the file's MD5 checksum before
applying it. This can take significantly longer than other commands,
particularly for large files.

For that reason, file uploads have a separate commit timeout:

```cpp
UploadFileOptions::commitTimeoutMs
```

The default value is 60 seconds, and it is independent of
`ConnectOptions::timeoutMs`.

When using the low-level transfer API directly, `Client::commitTransaction()`
also accepts its own `timeoutMs` parameter.

### API reference and example

The C++ API mirrors the [command groups](#command-groups) provided by the CLI.

See the following files for more information:

- [ElectraOneClient.hpp](include/electraone/ElectraOneClient.hpp) — complete API
  signatures
- [basic_usage.cpp](examples/basic_usage.cpp) — runnable example
- [examples/Makefile](examples/Makefile) — standalone example build

Build the library and run the example with:

```bash
cmake -B build
cmake --build build

cd examples
make
./basic_usage
```

### Linking with CMake

The recommended way to use the library from another CMake project is to include
this repository with `add_subdirectory()` or `FetchContent`, and then link
against the `electraone_api` target:

```cmake
target_link_libraries(your_app PRIVATE electraone_api)
```

The target automatically propagates the required RtMidi dependency and the
platform-specific MIDI libraries:

- macOS — CoreMIDI, CoreAudio, and CoreFoundation
- Linux — ALSA and pthreads
- Windows — WinMM

No additional link configuration is normally required.

The `basic_usage` example uses the same approach. See
[CMakeLists.txt](CMakeLists.txt) for the corresponding `add_executable()` and
`target_link_libraries()` calls.

### Linking without CMake

When linking manually, include:

1. `libelectraone.a` or `electraone.lib`
2. the RtMidi static library
3. the platform-specific MIDI libraries

#### macOS

```bash
c++ -std=c++17 \
  -I include \
  your_app.cpp \
  build/libelectraone.a \
  build/_deps/rtmidi-build/librtmidi.a \
  -framework CoreMIDI \
  -framework CoreAudio \
  -framework CoreFoundation \
  -o your_app
```

#### Linux

```bash
c++ -std=c++17 \
  -I include \
  your_app.cpp \
  build/libelectraone.a \
  build/_deps/rtmidi-build/librtmidi.a \
  -lasound \
  -lpthread \
  -o your_app
```

#### Windows

Run the following command from a Visual Studio Developer Command Prompt:

```bat
cl /std:c++17 /I include ^
  your_app.cpp ^
  build\Release\electraone.lib ^
  build\_deps\rtmidi-build\Release\rtmidi.lib ^
  winmm.lib ^
  /Fe:your_app.exe
```

The paths above assume a multi-configuration generator such as Visual Studio
and a Release build. Single-configuration generators, such as Ninja, may place
the libraries directly in the build directories without a `Release`
subdirectory.

The manual Windows command is provided as a best-effort example and has not been
verified on a Windows system. The CMake approach is recommended because it does
not depend on generator-specific output paths.

### Using a system-installed RtMidi

When RtMidi is provided by `find_package()` instead of being downloaded by the
project, replace the explicit RtMidi archive path with the system library:

- macOS or Linux: `-lrtmidi`
- Windows: `rtmidi.lib`

A system package may provide RtMidi as a shared library. In that case, the
resulting application may no longer be a fully self-contained binary. The
library type is determined by the installed RtMidi package and is outside this
project's control.
