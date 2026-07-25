#include <stdexcept>

#include "commands/command.hpp"
#include "commands/common.hpp"

void registerUiCommands(CLI::App &app, runner::Context &ctx)
{
    auto *ui = app.add_subcommand("ui", "Display/navigation operations");
    ui->require_subcommand(1);

    {
        static int page = 0;
        auto *sub = ui->add_subcommand("page-switch",
                                       "Switch Page (runtime, volatile)");
        sub->add_option("--page", page, "Page number")->required();
        sub->callback([&ctx] {
            runner::runAction(ctx, 0x09, 0x0A, { static_cast<uint8_t>(page) });
        });
    }
    {
        static int set = 0;
        auto *sub = ui->add_subcommand(
            "control-set-switch", "Switch Control Set (runtime, volatile)");
        sub->add_option("--set", set, "Control set ID")->required();
        sub->callback([&ctx] {
            runner::runAction(ctx, 0x09, 0x0B, { static_cast<uint8_t>(set) });
        });
    }
    {
        static std::string text;
        auto *sub = ui->add_subcommand(
            "bottom-bar-text",
            "Set Bottom Bar Text (runtime, max 40 characters)");
        sub->add_option("text", text, "Text to display")->required();
        sub->callback([&ctx] {
            if (text.size() > 40) {
                throw std::runtime_error(
                    "bottom bar text must be at most 40 characters");
            }
            runner::runAction(ctx, 0x14, 0x77, sysex::encodeAscii(text));
        });
    }
}
