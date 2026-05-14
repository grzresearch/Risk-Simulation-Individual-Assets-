#include "server/http_server.h"

#include "report/reporter.h"
#include "server/metrics.h"
#include "util/json_logger.h"
#include "util/logger.h"
#include "util/version.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <chrono>

namespace macro::server {

using nlohmann::json;

struct HttpServer::Impl {
    httplib::Server svr;
};

HttpServer::HttpServer(ServerConfig cfg, ScanFn scan, persist::Store* store)
    : cfg_(std::move(cfg)), scan_(std::move(scan)), store_(store),
      impl_(std::make_unique<Impl>()) {}

HttpServer::~HttpServer() = default;

namespace {

bool authorise(const httplib::Request& req, const std::string& token) {
    if (token.empty()) return true;
    auto auth = req.get_header_value("Authorization");
    return auth == ("Bearer " + token);
}

} // namespace

void HttpServer::run() {
    auto& svr = impl_->svr;
    const std::string token = cfg_.api_token;

    svr.set_keep_alive_max_count(20);
    svr.set_payload_max_length(64 * 1024 * 1024);

    svr.Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
        json j = {
            {"status", "ok"},
            {"version", version()},
            {"git_sha", git_sha()},
            {"build_date", build_date()},
        };
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/version", [](const httplib::Request&, httplib::Response& res) {
        json j = {{"version", version()}, {"git_sha", git_sha()}, {"build_date", build_date()}};
        res.set_content(j.dump(), "application/json");
    });

    svr.Get("/metrics", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(Metrics::instance().expose(), "text/plain; version=0.0.4");
    });

    svr.Get("/runs", [this, &token](const httplib::Request& req, httplib::Response& res) {
        if (!authorise(req, token)) { res.status = 401; return; }
        if (!store_) { res.status = 503; res.set_content("{\"error\":\"no store\"}", "application/json"); return; }
        auto ids = store_->list_runs(50);
        json arr = json::array();
        for (auto& id : ids) arr.push_back(id);
        res.set_content(arr.dump(), "application/json");
    });

    svr.Get(R"(/runs/([^/]+))", [this, &token](const httplib::Request& req, httplib::Response& res) {
        if (!authorise(req, token)) { res.status = 401; return; }
        if (!store_) { res.status = 503; return; }
        auto run = store_->load(req.matches[1].str());
        if (!run) { res.status = 404; return; }
        json out = json::parse(run->json_payload, nullptr, false);
        if (out.is_discarded()) out = run->json_payload;
        res.set_content(out.dump(), "application/json");
    });

    svr.Post("/scan", [this, &token](const httplib::Request& req, httplib::Response& res) {
        if (!authorise(req, token)) { res.status = 401; return; }
        Metrics::instance().inc("macro_scan_requests_total");
        auto t0 = std::chrono::steady_clock::now();
        try {
            auto out = scan_();
            auto body = report::to_json(out, false);
            auto dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            Metrics::instance().set("macro_last_scan_seconds", dt);
            Metrics::instance().set("macro_last_scan_assets", static_cast<double>(out.assets.size()));
            log::event("info", "scan.completed", {{"duration_s", dt},
                                                  {"assets", static_cast<long long>(out.assets.size())}});
            res.set_content(body, "application/json");
        } catch (const std::exception& e) {
            Metrics::instance().inc("macro_scan_errors_total");
            log::event("error", "scan.failed", {{"error", std::string(e.what())}});
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    log::info("HTTP server listening on " + cfg_.host + ":" + std::to_string(cfg_.port));
    running_ = true;
    svr.listen(cfg_.host.c_str(), cfg_.port);
    running_ = false;
}

void HttpServer::stop() {
    impl_->svr.stop();
}

} // namespace macro::server
