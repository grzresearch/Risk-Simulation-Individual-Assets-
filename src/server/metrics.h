#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <string>

namespace macro::server {

// Minimal Prometheus text-format exposition. Thread-safe counters and gauges.
class Metrics {
public:
    static Metrics& instance();

    void inc(const std::string& name, double v = 1.0,
             const std::map<std::string, std::string>& labels = {});
    void set(const std::string& name, double v,
             const std::map<std::string, std::string>& labels = {});

    std::string expose() const;

private:
    Metrics() = default;
    struct Entry {
        double value = 0.0;
        bool   is_counter = true;
        std::map<std::string, std::string> labels;
    };
    mutable std::mutex mu_;
    // key = name + label-fingerprint
    std::map<std::string, Entry> entries_;
};

} // namespace macro::server
