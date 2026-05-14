#include "persist/store.h"

#include "util/logger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace macro::persist {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

class JsonStore final : public Store {
public:
    explicit JsonStore(std::string dir) : dir_(std::move(dir)) {
        std::error_code ec;
        fs::create_directories(dir_, ec);
    }

    void save(const StoredRun& run) override {
        json j;
        j["run_id"]   = run.run_id;
        j["run_date"] = run.run_date;
        j["version"]  = run.version;
        j["git_sha"]  = run.git_sha;
        j["payload"]  = json::parse(run.json_payload, nullptr, false);
        if (j["payload"].is_discarded()) j["payload"] = run.json_payload;
        auto path = fs::path(dir_) / (sanitize(run.run_id) + ".json");
        std::ofstream f(path);
        f << j.dump(2);
        log::info("persist: wrote " + path.string());
    }

    std::optional<StoredRun> load(const std::string& run_id) const override {
        auto path = fs::path(dir_) / (sanitize(run_id) + ".json");
        std::ifstream f(path);
        if (!f) return std::nullopt;
        json j; f >> j;
        StoredRun r;
        r.run_id   = j.value("run_id", run_id);
        r.run_date = j.value("run_date", "");
        r.version  = j.value("version", "");
        r.git_sha  = j.value("git_sha", "");
        r.json_payload = j.value("payload", json::object()).dump();
        return r;
    }

    std::vector<std::string> list_runs(int limit) const override {
        std::vector<std::string> ids;
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(dir_, ec)) {
            if (e.path().extension() != ".json") continue;
            ids.push_back(e.path().stem().string());
        }
        std::sort(ids.begin(), ids.end(), std::greater<>());
        if (static_cast<int>(ids.size()) > limit) ids.resize(static_cast<std::size_t>(limit));
        return ids;
    }

    std::string kind() const override { return "json-file"; }

private:
    std::string dir_;
    static std::string sanitize(const std::string& s) {
        std::string out; out.reserve(s.size());
        for (char c : s) out.push_back((c == '/' || c == '\\' || c == ':') ? '_' : c);
        return out;
    }
};

} // namespace

std::unique_ptr<Store> make_json_store(const std::string& dir) {
    return std::make_unique<JsonStore>(dir);
}

} // namespace macro::persist
