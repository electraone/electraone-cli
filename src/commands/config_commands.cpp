#include "commands/command.hpp"
#include "commands/common.hpp"

void registerConfigCommands(CLI::App& app, runner::Context& ctx) {
    auto* config = app.add_subcommand("config", "System configuration operations");
    config->require_subcommand(1);

    config->add_subcommand("get", "Get Configuration JSON")
        ->callback([&ctx] { runner::runQuery(ctx, 0x02, 0x02, {}); });

    {
        static std::string file;
        auto* sub = config->add_subcommand("upload", "Upload Configuration from a JSON file (use - for stdin)");
        sub->add_option("file", file, "Configuration JSON file")->required();
        sub->callback([&ctx] {
            auto content = runner::readFileOrStdin(file);
            runner::runAction(ctx, 0x01, 0x02, sysex::encodeAscii(content));
        });
    }
    config->add_subcommand("remove", "Remove Config")->callback([&ctx] { runner::runAction(ctx, 0x05, 0x02, {}); });
}
