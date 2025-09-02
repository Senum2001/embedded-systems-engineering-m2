/**
 * @file http_client.cpp
 * @brief Implementation of HTTP client
 * @author EcoWatt Team  
 * @date 2025-09-02
 */

#include "http_client.hpp"
#include "logger.hpp"
#include <sstream>
#include <algorithm>

namespace ecoWatt {

HttpClient::HttpClient(const std::string& base_url, uint32_t timeout_ms)
    : base_url_(base_url), timeout_ms_(timeout_ms) {
    
    // Initialize libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_ = curl_easy_init();
    
    if (!curl_) {
        throw HttpException("Failed to initialize CURL");
    }
    
    setupCurl();
    LOG_DEBUG("HTTP client initialized with base URL: {}", base_url_);
}

HttpClient::~HttpClient() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
    curl_global_cleanup();
}

void HttpClient::setupCurl() {
    // Basic options
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_ / 2);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, 3L);
    
    // SSL options
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
    
    // Write callback
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
    
    // User agent
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, "EcoWatt-Device/2.0");
}

HttpResponse HttpClient::post(const std::string& endpoint,
                             const std::string& data,
                             const std::map<std::string, std::string>& headers) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string url = base_url_ + endpoint;
    std::string response_body;
    
    // Setup URL and data
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    
    // Combine default headers with request headers
    auto all_headers = default_headers_;
    for (const auto& [key, value] : headers) {
        all_headers[key] = value;
    }
    
    // Add headers
    struct curl_slist* header_list = addHeaders(all_headers);
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);
    
    LOG_TRACE("POST {}: {}", url, data);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl_);
    
    // Cleanup headers
    if (header_list) {
        curl_slist_free_all(header_list);
    }
    
    HttpResponse response;
    response.body = response_body;
    
    if (res != CURLE_OK) {
        std::string error_msg = curl_easy_strerror(res);
        LOG_ERROR("CURL error: {}", error_msg);
        throw HttpException("Request failed: " + error_msg);
    }
    
    // Get response code
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response.status_code);
    
    LOG_TRACE("Response {}: {}", response.status_code, response.body);
    
    if (!response.isSuccess()) {
        LOG_WARN("HTTP error {}: {}", response.status_code, response.body);
    }
    
    return response;
}

HttpResponse HttpClient::get(const std::string& endpoint,
                            const std::map<std::string, std::string>& headers) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string url = base_url_ + endpoint;
    std::string response_body;
    
    // Setup URL for GET
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    
    // Combine default headers with request headers
    auto all_headers = default_headers_;
    for (const auto& [key, value] : headers) {
        all_headers[key] = value;
    }
    
    // Add headers
    struct curl_slist* header_list = addHeaders(all_headers);
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);
    
    LOG_TRACE("GET {}", url);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl_);
    
    // Cleanup headers
    if (header_list) {
        curl_slist_free_all(header_list);
    }
    
    HttpResponse response;
    response.body = response_body;
    
    if (res != CURLE_OK) {
        std::string error_msg = curl_easy_strerror(res);
        LOG_ERROR("CURL error: {}", error_msg);
        throw HttpException("Request failed: " + error_msg);
    }
    
    // Get response code
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response.status_code);
    
    LOG_TRACE("Response {}: {}", response.status_code, response.body);
    
    return response;
}

void HttpClient::setDefaultHeaders(const std::map<std::string, std::string>& headers) {
    default_headers_ = headers;
    LOG_DEBUG("Updated default headers (count: {})", headers.size());
}

void HttpClient::setTimeout(uint32_t timeout_ms) {
    timeout_ms_ = timeout_ms;
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_ / 2);
    LOG_DEBUG("HTTP timeout set to {}ms", timeout_ms_);
}

void HttpClient::setSSLVerification(bool enable) {
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, enable ? 1L : 0L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, enable ? 2L : 0L);
    LOG_DEBUG("SSL verification {}", enable ? "enabled" : "disabled");
}

size_t HttpClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append(static_cast<char*>(contents), total_size);
    return total_size;
}

struct curl_slist* HttpClient::addHeaders(const std::map<std::string, std::string>& headers) {
    struct curl_slist* header_list = nullptr;
    
    for (const auto& [key, value] : headers) {
        std::string header = key + ": " + value;
        header_list = curl_slist_append(header_list, header.c_str());
    }
    
    return header_list;
}

} // namespace ecoWatt
