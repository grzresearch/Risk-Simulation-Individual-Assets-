#include "server/metrics.h"

#include <sstream>

namespace macro::server {

Metrics& Metrics::instance() { static Metrics m; return m; }

namespace {

std::string fingerprint(const std::string& name,
                        const std::map<std::string, std::string>& labels) {
    std::ostringstream o;
    o << name << '{';
    bool first = true;
    for (const auto& [k, v] : labels) {
        if (!first) o << ',';
        o << k << '=' << '"' << v << '"';
        first = false;
    }
    o << '}';
    return o.str();
}

std::string label_block(const std::map<std::string, std::string>& labels) {
    if (labels.empty()) return "";
    std::ostringstream o;
    o << '{';
    bool first = true;
    for (const auto& [k, v] : labels) {
        if (!first) o << ',';
        o << k << "=\"" << v << "\"";
        first = false;
    }
    o << '}';
    return o.str();
}

} // namespace

void Metrics::inc(const std::string& name, double v,
                  const std::map<std::string, std::string>& labels) {
    auto key = fingerprint(name, labels);
    std::lock_guard<std::mutex> lk(mu_);
    auto& e = entries_[key];
    e.value     += v;
    e.is_counter = true;
    e.labels     = labels;
}

void Metrics::set(const std::string& name, double v,
                  const std::map<std::string, std::string>& labels) {
    auto key = fingerprint(name, labels);
    std::lock_guard<std::mutex> lk(mu_);
    auto& e = entries_[key];
    e.value      = v;
    e.is_counter = false;
    e.labels     = labels;
}

std::string Metrics::expose() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::ostringstream o;
    std::map<std::string, std::string> seen;
    for (const auto& [k, e] : entries_) {
        // strip label fingerprint to recover metric name
        auto br = k.find('{');
        std::string name = (br == std::string::npos) ? k : k.substr(0, br);
        if (!seen.count(name)) {
            o << "# TYPE " << name << ' ' << (e.is_counter ? "counter" : "gauge") << "\n";
            seen[name] = name;
        }
        o << name << label_block(e.labels) << ' ' << e.value << "\n";
    }
    return o.str();
}

} // namespace macro::server
