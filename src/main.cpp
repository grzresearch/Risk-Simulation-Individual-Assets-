#include "config.h"
#include "data/coingecko_client.h"
#include "data/consensus_provider.h"
#include "data/econ_calendar.h"
#include "data/fred_client.h"
#include "data/price_repository.h"
#include "data/yahoo_client.h"
#include "engine/scanner.h"
#include "report/reporter.h"
#include "util/logger.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void print_usage() {
    std::cout <<
        "macro_risk - Live macro-data driven directional risk engine\n"
        "\n"
        "Usage:\n"
        "  macro_risk --config <path>            Run scan with config file\n"
        "  macro_risk --init   <path>            Write a default config skeleton\n"
        "  macro_risk --config <path> --json     Emit JSON instead of text\n"
        "  macro_risk --config <path> --debug    Verbose logging\n"
        "\n"
        "Environment:\n"
        "  FRED_API_KEY    Used if config.api_keys.fred is empty/placeholder.\n"
        "\n"
        "Config:  see config.example.json. Each asset specifies a yahoo symbol\n"
        "         and/or a coingecko id. The engine fetches history, aligns it\n"
        "         with past macro releases (CPI, PPI, NFP, FOMC, PCE, ...) and\n"
        "         computes the empirical conditional return distribution.\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string config_path;
    std::string init_path;
    bool emit_json = false;
    bool debug = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--config" && i + 1 < argc) config_path = argv[++i];
        else if (a == "--init" && i + 1 < argc) init_path = argv[++i];
        else if (a == "--json") emit_json = true;
        else if (a == "--debug") debug = true;
        else if (a == "-h" || a == "--help") { print_usage(); return 0; }
        else {
            std::cerr << "Unknown arg: " << a << "\n";
            print_usage();
            return 2;
        }
    }

    if (!init_path.empty()) {
        macro::write_default_config(init_path);
        std::cout << "Wrote default config to " << init_path << "\n";
        return 0;
    }
    if (config_path.empty()) {
        print_usage();
        return 2;
    }
    if (debug) macro::log::Logger::instance().set_level(macro::log::Level::Debug);

    try {
        auto cfg = macro::load_config(config_path);

        if (cfg.api_keys.fred.empty() || cfg.api_keys.fred == "REPLACE_WITH_FRED_KEY") {
            const char* env = std::getenv("FRED_API_KEY");
            if (env && std::strlen(env) > 0) cfg.api_keys.fred = env;
        }
        if (cfg.api_keys.fred.empty()) {
            macro::log::warn("FRED API key not set. Calendar will use fallback heuristics "
                             "and historical releases will be empty.");
        }

        macro::data::FredClient        fred(cfg.api_keys.fred);
        macro::data::YahooClient       yahoo;
        macro::data::CoinGeckoClient   gecko;
        macro::data::EconCalendar      cal(fred);
        macro::data::PriceRepository   repo(yahoo, gecko, cfg.cache_dir);
        macro::data::ConsensusProvider consensus(cfg.cache_dir);

        macro::engine::Scanner scanner(cfg, fred, cal, repo, consensus);
        auto result = scanner.run();

        if (emit_json) {
            std::cout << macro::report::to_json(result) << "\n";
        } else {
            macro::report::print_text(std::cout, result);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
