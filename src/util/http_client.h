#pragma once

#include <map>
#include <stdexcept>
#include <string>

namespace macro::http {

struct Response {
    long status = 0;
    std::string body;
    bool ok() const { return status >= 200 && status < 300; }
};

class HttpError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Simple GET with optional query params and headers. Throws HttpError on
// transport failure. Non-2xx responses are returned with status set.
Response get(const std::string& url,
             const std::map<std::string, std::string>& params = {},
             const std::map<std::string, std::string>& headers = {},
             long timeout_seconds = 20);

std::string url_encode(const std::string& s);

} // namespace macro::http
