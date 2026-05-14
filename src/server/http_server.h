#pragma once

#include "engine/scanner.h"
#include "persist/store.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace macro::server {

struct ServerConfig {
    std::string host = "0.0.0.0";
    int         port = 8080;
    std::string api_token; // optional bearer token; if empty, no auth
};

// REST endpoints:
//   GET  /healthz        - 200 OK with build info
//   GET  /metrics        - Prometheus text exposition
//   GET  /version        - JSON build version
//   GET  /runs           - list latest persisted run ids
//   GET  /runs/{id}      - load one run by id
//   POST /scan           - trigger a fresh scan (uses provided scanner factory)
class HttpServer {
public:
    using ScanFn = std::function<engine::ScanOutput()>;

    HttpServer(ServerConfig cfg, ScanFn scan, persist::Store* store);
    ~HttpServer();

    void run();   // blocks
    void stop();

private:
    ServerConfig    cfg_;
    ScanFn          scan_;
    persist::Store* store_;
    std::atomic<bool> running_{false};
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace macro::server
