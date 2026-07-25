#include "commands/command.hpp"
#include "commands/common.hpp"

void registerWindowCommands(CLI::App &app, runner::Context &ctx)
{
    auto *window = app.add_subcommand("window", "Control Window Repaints");
    window->require_subcommand(1);

    window->add_subcommand("stop", "Stop window repaints")->callback([&ctx] {
        runner::runAction(ctx, 0x7F, 0x7A, { 0x00, 0x00 });
    });
    window->add_subcommand("resume", "Resume window repaints")
        ->callback(
            [&ctx] { runner::runAction(ctx, 0x7F, 0x7A, { 0x01, 0x00 }); });
}
