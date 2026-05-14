#pragma once

#include <map>
#include <string>

namespace macro::governance {

// Append-only audit log emitting one JSON object per line. Records who did
// what, when, against which run/version. Designed to be ingested by SIEM
// pipelines (Splunk, ELK, Loki).
class AuditLog {
public:
    explicit AuditLog(std::string path);

    // Action examples: "scan.run", "scan.fetch", "report.export",
    // "alert.dispatch", "store.write", "store.read".
    void record(const std::string& actor,
                const std::string& action,
                const std::map<std::string, std::string>& fields = {});

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

} // namespace macro::governance
