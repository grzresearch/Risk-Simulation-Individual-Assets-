#include "util/http_client.h"

#include "util/logger.h"

#include <curl/curl.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <sstream>

namespace macro::http {

namespace {

class CurlGlobal {
public:
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

void ensure_curl_init() {
    static CurlGlobal g;
    (void)g;
}

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string build_query(const std::map<std::string, std::string>& params) {
    if (params.empty()) return {};
    std::ostringstream oss;
    bool first = true;
    for (const auto& [k, v] : params) {
        oss << (first ? "?" : "&") << url_encode(k) << "=" << url_encode(v);
        first = false;
    }
    return oss.str();
}

} // namespace

std::string url_encode(const std::string& s) {
    ensure_curl_init();
    CURL* tmp = curl_easy_init();
    if (!tmp) return s;
    char* esc = curl_easy_escape(tmp, s.c_str(), static_cast<int>(s.size()));
    std::string out = esc ? esc : s;
    if (esc) curl_free(esc);
    curl_easy_cleanup(tmp);
    return out;
}

Response get(const std::string& url,
             const std::map<std::string, std::string>& params,
             const std::map<std::string, std::string>& headers,
             long timeout_seconds) {
    ensure_curl_init();
    CURL* curl = curl_easy_init();
    if (!curl) throw HttpError("curl init failed");

    const std::string full = url + build_query(params);
    Response resp;
    char err[CURL_ERROR_SIZE] = {0};

    curl_easy_setopt(curl, CURLOPT_URL, full.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "macro-risk-engine/0.1 (+libcurl)");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    curl_slist* hdr = nullptr;
    for (const auto& [k, v] : headers) {
        hdr = curl_slist_append(hdr, (k + ": " + v).c_str());
    }
    if (hdr) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr);

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);

    if (hdr) curl_slist_free_all(hdr);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        throw HttpError(std::string("curl error: ") + (std::strlen(err) ? err : curl_easy_strerror(rc)));
    }
    if (!resp.ok()) {
        log::warn("HTTP " + std::to_string(resp.status) + " for " + url);
    }
    return resp;
}

} // namespace macro::http
