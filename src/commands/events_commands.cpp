#include "commands/command.hpp"
#include "commands/common.hpp"
#include "commands/event_decoder.hpp"

void registerEventsCommands(CLI::App &app, runner::Context &ctx)
{
    auto *events = app.add_subcommand("events", "Controller event operations");
    events->require_subcommand(1);

    {
        static std::string port;
        auto *sub = events->add_subcommand("set-port", "Set Events MIDI Port");
        sub->add_option("--port", port, "port1, port2, or ctrl")->required();
        sub->callback([&ctx] {
            runner::runAction(
                ctx, 0x14, 0x7B, { commands::parsePortSelector(port) });
        });
    }
    {
        static bool page = false, controlSet = false, usbHost = false,
                    pots = false, touch = false, button = false, window = false,
                    all = false;
        auto *sub = events->add_subcommand(
            "subscribe",
            "Subscribe Events: select which event types the device sends");
        commands::addSubscribeFlags(
            sub, page, controlSet, usbHost, pots, touch, button, window, all);
        sub->callback([&ctx] {
            uint8_t flags = commands::subscribeFlagsToByte(
                page, controlSet, usbHost, pots, touch, button, window, all);
            runner::runAction(ctx, 0x14, 0x79, { flags });
        });
    }
    {
        static bool page = false, controlSet = false, usbHost = false,
                    pots = false, touch = false, button = false, window = false,
                    all = true;
        static int duration = 0;
        auto *sub = events->add_subcommand(
            "listen",
            "Subscribe then print decoded events until Ctrl+C or --duration elapses (default: all events)");
        commands::addSubscribeFlags(
            sub, page, controlSet, usbHost, pots, touch, button, window, all);
        sub->add_option("--duration",
                        duration,
                        "Stop after this many seconds (0 = run forever)")
            ->default_val(0);
        sub->callback([&ctx] {
            uint8_t flags = commands::subscribeFlagsToByte(
                page, controlSet, usbHost, pots, touch, button, window, all);
            auto msg = sysex::buildMessage(0x14, 0x79, { flags }, ctx.txnId);
            runner::listen(
                ctx, { msg }, duration, commands::printEventWithTimestamp);
        });
    }
}
