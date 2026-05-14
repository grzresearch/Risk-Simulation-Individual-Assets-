#pragma once

#include "engine/scanner.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace macro::persist {

struct StoredRun {
    std::string  run_id;        // ISO timestamp or UUID-like
    std::string  run_date;      // YYYY-MM-DD
    std::string  version;       // engine semver
    std::string  git_sha;       // build provenance
    std::string  json_payload;  // full ScanOutput as JSON
};

// Storage abstraction. Implementations must be safe for serial calls from
// one process. They are not required to be cross-process safe.
class Store {
public:
    virtual ~Store() = default;
    virtual void save(const StoredRun& run) = 0;
    virtual std::optional<StoredRun> load(const std::string& run_id) const = 0;
    virtual std::vector<std::string> list_runs(int limit = 50) const = 0;
    virtual std::string kind() const = 0;
};

std::unique_ptr<Store> make_json_store(const std::string& dir);
std::unique_ptr<Store> make_postgres_store(const std::string& dsn); // returns null if libpq unavailable

} // namespace macro::persist
