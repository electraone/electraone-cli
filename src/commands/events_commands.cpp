#include <iostream>

#include "commands/command.hpp"
#include "commands/common.hpp"
#include "commands/event_decoder.hpp"

namespace {

// Adds the Subscribe Events flag options (--page, --control-set, ...,
// --all) to sub, writing into the given bool references.
void addSubscribeFlags(CLI::App* sub, bool& page, bool& controlSet, bool& usbHost, bool& pots, bool& touch,
                        bool& button, bool& window, bool& all) {
    sub->add_flag("--page", page, "Subscribe to Page Switch events");
    sub->add_flag("--control-set", controlSet, "Subscribe to Control Set Switch events");
    sub->add_flag("--usb-host", usbHost, "Subscribe to USB Host Change events");
    sub->add_flag("--pots", pots, "Subscribe to Pots events");
    sub->add_flag("--touch", touch, "Subscribe to Touch events");
    sub->add_flag("--button", button, "Subscribe to Button events");
    sub->add_flag("--window", window, "Subscribe to Window events");
    sub->add_flag("--all", all, "Subscribe to all event types");
}

uint8_t buildFlags(bool page, bool controlSet, bool usbHost, bool pots, bool touch, bool button, bool window,
                    bool all) {
    if (all) return 0x7F;
    uint8_t flags = 0;
    if (page) flags |= 1 << 0;
    if (controlSet) flags |= 1 << 1;
    if (usbHost) flags |= 1 << 2;
    if (pots) flags |= 1 << 3;
    if (touch) flags |= 1 << 4;
    if (button) flags |= 1 << 5;
    if (window) flags |= 1 << 6;
    return flags;
}

}  // namespace

void registerEventsCommands(CLI::App& app, runner::Context& ctx) {
    auto* events = app.add_subcommand("events", "Controller event operations");
    events->require_subcommand(1);

    {
        static std::string port;
        auto* sub = events->add_subcommand("set-port", "Set Events MIDI Port");
        sub->add_option("--port", port, "port1, port2, or ctrl")->required();
        sub->callback(
            [&ctx] { runner::runAction(ctx, 0x14, 0x7B, {commands::parsePortSelector(port)}); });
    }
    {
        static bool page = false, controlSet = false, usbHost = false, pots = false, touch = false,
                    button = false, window = false, all = false;
        auto* sub = events->add_subcommand("subscribe", "Subscribe Events: select which event types the device sends");
        addSubscribeFlags(sub, page, controlSet, usbHost, pots, touch, button, window, all);
        sub->callback([&ctx] {
            uint8_t flags = buildFlags(page, controlSet, usbHost, pots, touch, button, window, all);
            runner::runAction(ctx, 0x14, 0x79, {flags});
        });
    }
    {
        static bool page = false, controlSet = false, usbHost = false, pots = false, touch = false,
                    button = false, window = false, all = true;
        static int duration = 0;
        auto* sub = events->add_subcommand(
            "listen", "Subscribe then print decoded events until Ctrl+C or --duration elapses (default: all events)");
        addSubscribeFlags(sub, page, controlSet, usbHost, pots, touch, button, window, all);
        sub->add_option("--duration", duration, "Stop after this many seconds (0 = run forever)")->default_val(0);
        sub->callback([&ctx] {
            uint8_t flags = buildFlags(page, controlSet, usbHost, pots, touch, button, window, all);
            auto msg = sysex::buildMessage(0x14, 0x79, {flags}, ctx.txnId);
            runner::listen(ctx, {msg}, duration,
                            [](const sysex::ParsedResponse& r) { std::cout << commands::describeEvent(r) << "\n"; });
        });
    }
}
