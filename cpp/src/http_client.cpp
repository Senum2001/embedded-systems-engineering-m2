/**
 * @file http_client.cpp
 * @brief Implementation of HTTP client using cpprestsdk
 * @author EcoWatt Team  
 * @date 2025-09-02
 */

#include "http_client.hpp"
#include "logger.hpp"
#include <cpprest/http_client.h>

using namespace web;
using namespace web::http;
using namespace web::http::client;

namespace ecoWatt {

HttpClient::HttpClient(const std::string& base_url, uint32_t timeout_ms)
    : base_url_(base_url), timeout_ms_(timeout_ms) {
    
    try {
        // Configure client
        http_client_config config;
        config.set_timeout(utility::seconds(timeout_ms_ / 1000));
        
        // Create HTTP client
        client_ = std::make_unique<http_client>(utility::conversions::to_string_t(base_url_), config);
        
        LOG_DEBUG("HTTP client initialized with base URL: {}", base_url_);
    } catch (const std::exception& e) {
        throw HttpException("Failed to initialize HTTP client: " + std::string(e.what()));
    }
}

HttpClient::~HttpClient() {
    // Cleanup handled by unique_ptr
}

HttpResponse HttpClient::post(const std::string& endpoint, 
                             const std::string& data,
                             const std::map<std::string, std::string>& headers) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        LOG_TRACE("POST request to endpoint: {}", endpoint);
        
        // Create request
        http_request request(methods::POST);
        request.set_request_uri(utility::conversions::to_string_t(endpoint));
        
        // Set headers
        auto http_headers = buildHeaders(headers);
        for (const auto& header : http_headers) {
            request.headers().add(header.first, header.second);
        }
        
        // Set body
        if (!data.empty()) {
            request.set_body(utility::conversions::to_string_t(data), U("application/json"));
        }
        
        // Send request
        auto response = client_->request(request).get();
        
        LOG_TRACE("POST response status: {}", response.status_code());
        
        return convertResponse(response);
        
    } catch (const std::exception& e) {
        std::string error_msg = "POST request failed: " + std::string(e.what());
        LOG_ERROR(error_msg);
        throw HttpException(error_msg);
    }
}

HttpResponse HttpClient::get(const std::string& endpoint,
                            const std::map<std::string, std::string>& headers) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        LOG_TRACE("GET request to endpoint: {}", endpoint);
        
        // Create request
        http_request request(methods::GET);
        request.set_request_uri(utility::conversions::to_string_t(endpoint));
        
        // Set headers
        auto http_headers = buildHeaders(headers);
        for (const auto& header : http_headers) {
            request.headers().add(header.first, header.second);
        }
        
        // Send request
        auto response = client_->request(request).get();
        
        LOG_TRACE("GET response status: {}", response.status_code());
        
        return convertResponse(response);
        
    } catch (const std::exception& e) {
        std::string error_msg = "GET request failed: " + std::string(e.what());
        LOG_ERROR(error_msg);
        throw HttpException(error_msg);
    }
}

void HttpClient::setDefaultHeaders(const std::map<std::string, std::string>& headers) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_headers_ = headers;
    LOG_DEBUG("Set {} default headers", headers.size());
}

void HttpClient::setTimeout(uint32_t timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    timeout_ms_ = timeout_ms;
    
    // Recreate client with new timeout
    http_client_config config;
    config.set_timeout(utility::seconds(timeout_ms_ / 1000));
    client_ = std::make_unique<http_client>(utility::conversions::to_string_t(base_url_), config);
    
    LOG_DEBUG("HTTP client timeout set to {} ms", timeout_ms_);
}

void HttpClient::setSSLVerification(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Note: cpprestsdk SSL verification is handled differently
    // This is a placeholder for compatibility
    LOG_DEBUG("SSL verification {}", enable ? "enabled" : "disabled");
}

http_headers HttpClient::buildHeaders(const std::map<std::string, std::string>& headers) {
    http_headers result;
    
    // Add default headers
    for (const auto& header : default_headers_) {
        result.add(utility::conversions::to_string_t(header.first), 
                  utility::conversions::to_string_t(header.second));
    }
    
    // Add request-specific headers (override defaults)
    for (const auto& header : headers) {
        result.add(utility::conversions::to_string_t(header.first), 
                  utility::conversions::to_string_t(header.second));
    }
    
    return result;
}

HttpResponse HttpClient::convertResponse(const http_response& response) {
    HttpResponse result;
    
    // Status code
    result.status_code = response.status_code();
    
    // Body
    auto body_task = response.extract_string();
    auto body_string = body_task.get();
    result.body = utility::conversions::to_utf8string(body_string);
    
    // Headers
    for (const auto& header : response.headers()) {
        std::string key = utility::conversions::to_utf8string(header.first);
        std::string value = utility::conversions::to_utf8string(header.second);
        result.headers[key] = value;
    }
    
    return result;
}

} // namespace ecoWatt
