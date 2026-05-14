#include "alerts/slack_alerter.h"
#include "config.h"
#include "data/coingecko_client.h"
#include "data/consensus_provider.h"
#include "data/econ_calendar.h"
#include "data/fred_client.h"
#include "data/price_repository.h"
#include "data/yahoo_client.h"
#include "engine/scanner.h"
#include "governance/audit_log.h"
#include "governance/model_card.h"
#include "persist/store.h"
#include "report/csv_reporter.h"
#include "report/html_reporter.h"
#include "report/reporter.h"
#include "server/http_server.h"
#include "server/metrics.h"
#include "util/json_logger.h"
#include "util/logger.h"
#include "util/version.h"

#include <chrono>
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
        "  macro_risk --config <path>             Run scan (text)\n"
        "  macro_risk --init   <path>             Write default config\n"
        "  macro_risk --config <p> --json         Emit JSON\n"
        "  macro_risk --config <p> --csv  <out>   Emit CSV\n"
        "  macro_risk --config <p> --html <out>   Emit standalone HTML\n"
        "  macro_risk --config <p> --model-card <out>  Generate Markdown model card\n"
        "  macro_risk --config <p> --persist      Persist run + record audit\n"
        "  macro_risk --config <p> --alert        Evaluate alerts and dispatch (Slack)\n"
        "  macro_risk --config <p> --serve        Run scan & launch HTTP server\n"
        "  macro_risk --version                   Print version + git\n"
        "  macro_risk --config <p> --debug        Verbose logging\n"
        "\n"
        "Environment:\n"
        "  FRED_API_KEY    Used if config.api_keys.fred is empty.\n"
        "  SLACK_WEBHOOK   Used if config.alerts.slack_webhook is empty.\n"
        "  POSTGRES_DSN    Used if config.persistence.postgres_dsn is empty.\n";
}

std::string now_run_id() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{}; gmtime_r(&t, &tm);
    char buf[32]; std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", &tm);
    return std::string(buf);
}

} // namespace

int main(int argc, char** argv) {
    std::string config_path;
    std::string init_path;
    std::string csv_out, html_out, model_card_out;
    bool emit_json = false;
    bool debug = false;
    bool do_persist = false;
    bool do_alert = false;
    bool do_serve = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto needs = [&](const char* opt) {
            if (i + 1 >= argc) { std::cerr << opt << " needs an argument\n"; std::exit(2); }
        };
        if (a == "--config")            { needs("--config"); config_path = argv[++i]; }
        else if (a == "--init")         { needs("--init");   init_path   = argv[++i]; }
        else if (a == "--csv")          { needs("--csv");    csv_out     = argv[++i]; }
        else if (a == "--html")         { needs("--html");   html_out    = argv[++i]; }
        else if (a == "--model-card")   { needs("--model-card"); model_card_out = argv[++i]; }
        else if (a == "--json")         emit_json = true;
        else if (a == "--debug")        debug = true;
        else if (a == "--persist")      do_persist = true;
        else if (a == "--alert")        do_alert = true;
        else if (a == "--serve")        do_serve = true;
        else if (a == "--version")      { std::cout << macro::version_string() << "\n"; return 0; }
        else if (a == "-h" || a == "--help") { print_usage(); return 0; }
        else { std::cerr << "Unknown arg: " << a << "\n"; print_usage(); return 2; }
    }

    if (!init_path.empty()) {
        macro::write_default_config(init_path);
        std::cout << "Wrote default config to " << init_path << "\n";
        return 0;
    }
    if (config_path.empty()) { print_usage(); return 2; }
    if (debug) macro::log::Logger::instance().set_level(macro::log::Level::Debug);

    try {
        auto cfg = macro::load_config(config_path);

        if (cfg.api_keys.fred.empty() || cfg.api_keys.fred == "REPLACE_WITH_FRED_KEY") {
            if (const char* env = std::getenv("FRED_API_KEY")) cfg.api_keys.fred = env;
        }
        if (cfg.alerts.slack_webhook.empty()) {
            if (const char* env = std::getenv("SLACK_WEBHOOK")) cfg.alerts.slack_webhook = env;
        }
        if (cfg.persist.postgres_dsn.empty()) {
            if (const char* env = std::getenv("POSTGRES_DSN")) cfg.persist.postgres_dsn = env;
        }

        if (cfg.structured_logging) macro::log::JsonLogger::instance().enable(true);

        macro::governance::AuditLog audit(cfg.audit.path);
        auto audit_evt = [&](const std::string& action,
                             const std::map<std::string, std::string>& f = {}) {
            if (cfg.audit.enabled) audit.record(cfg.audit.actor, action, f);
        };
        audit_evt("startup", {{"version", macro::version()}, {"git", macro::git_sha()}});

        // Persistence
        std::unique_ptr<macro::persist::Store> store;
        if (do_persist || do_serve) {
            if (cfg.persist.backend == "postgres" && !cfg.persist.postgres_dsn.empty()) {
                store = macro::persist::make_postgres_store(cfg.persist.postgres_dsn);
            }
            if (!store) store = macro::persist::make_json_store(cfg.persist.json_dir);
            macro::log::info("persistence backend: " + store->kind());
        }

        macro::data::FredClient        fred(cfg.api_keys.fred);
        macro::data::YahooClient       yahoo;
        macro::data::CoinGeckoClient   gecko;
        macro::data::EconCalendar      cal(fred);
        macro::data::PriceRepository   repo(yahoo, gecko, cfg.cache_dir);
        macro::data::ConsensusProvider consensus(cfg.cache_dir);

        auto run_scan = [&]() {
            macro::engine::Scanner scanner(cfg, fred, cal, repo, consensus);
            auto t0 = std::chrono::steady_clock::now();
            auto out = scanner.run();
            auto dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            macro::server::Metrics::instance().set("macro_last_scan_seconds", dt);
            macro::server::Metrics::instance().inc("macro_scans_total");
            audit_evt("scan.completed",
                {{"assets", std::to_string(out.assets.size())},
                 {"duration_s", std::to_string(dt)}});
            return out;
        };

        // Server mode loops forever
        if (do_serve) {
            macro::server::ServerConfig sc;
            sc.host      = cfg.server.host;
            sc.port      = cfg.server.port;
            sc.api_token = cfg.server.api_token;
            macro::server::HttpServer server(sc, run_scan, store.get());
            audit_evt("server.start", {{"host", sc.host}, {"port", std::to_string(sc.port)}});
            server.run();
            return 0;
        }

        auto result = run_scan();

        // Outputs
        if (emit_json) {
            std::cout << macro::report::to_json(result) << "\n";
        } else if (!csv_out.empty()) {
            std::ofstream f(csv_out);
            macro::report::to_csv(f, result);
            audit_evt("report.export", {{"format", "csv"}, {"path", csv_out}});
        } else if (!html_out.empty()) {
            std::ofstream f(html_out);
            macro::report::to_html(f, result);
            audit_evt("report.export", {{"format", "html"}, {"path", html_out}});
        } else {
            macro::report::print_text(std::cout, result);
        }

        if (!model_card_out.empty()) {
            std::ofstream f(model_card_out);
            macro::governance::write_model_card(f, cfg, result);
            audit_evt("model_card.write", {{"path", model_card_out}});
        }

        if (do_persist && store) {
            macro::persist::StoredRun r;
            r.run_id   = now_run_id();
            r.run_date = macro::time::to_iso(macro::time::today_utc());
            r.version  = macro::version();
            r.git_sha  = macro::git_sha();
            r.json_payload = macro::report::to_json(result, false);
            store->save(r);
            audit_evt("store.write", {{"run_id", r.run_id}, {"backend", store->kind()}});
        }

        if (do_alert && cfg.alerts.enabled) {
            macro::alerts::AlertRule rule;
            rule.prob_threshold  = cfg.alerts.prob_threshold;
            rule.bh_q_threshold  = cfg.alerts.bh_q_threshold;
            rule.min_sample_size = cfg.alerts.min_sample_size;
            auto items = macro::alerts::evaluate(result, rule);
            macro::server::Metrics::instance().set("macro_alerts_pending",
                                                   static_cast<double>(items.size()));
            if (!items.empty() && !cfg.alerts.slack_webhook.empty()) {
                bool ok = macro::alerts::send_slack(cfg.alerts.slack_webhook, items);
                macro::server::Metrics::instance().inc(ok ? "macro_alerts_sent_total"
                                                          : "macro_alerts_failed_total",
                                                       static_cast<double>(items.size()));
                audit_evt("alert.dispatch",
                    {{"count", std::to_string(items.size())},
                     {"ok", ok ? "true" : "false"}});
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
