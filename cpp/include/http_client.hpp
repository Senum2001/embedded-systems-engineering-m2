/**
 * @file http_client.hpp
 * @brief HTTP client for API communication
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

#include "types.hpp"
#include "exceptions.hpp"
#include <string>
#include <map>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <mutex>

namespace ecoWatt {

/**
 * @brief HTTP response structure
 */
struct HttpResponse {
    long status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;
    
    bool isSuccess() const { return status_code >= 200 && status_code < 300; }
};

/**
 * @brief HTTP client for REST API communication using cpprestsdk
 */
class HttpClient {
public:
    /**
     * @brief Constructor
     * @param base_url Base URL for API
     * @param timeout_ms Request timeout in milliseconds
     */
    HttpClient(const std::string& base_url, uint32_t timeout_ms = 5000);

    /**
     * @brief Destructor
     */
    ~HttpClient();

    /**
     * @brief Perform POST request
     * @param endpoint API endpoint
     * @param data Request body data
     * @param headers Additional headers
     * @return HTTP response
     * @throws HttpException on error
     */
    HttpResponse post(const std::string& endpoint, 
                     const std::string& data,
                     const std::map<std::string, std::string>& headers = {});

    /**
     * @brief Perform GET request
     * @param endpoint API endpoint
     * @param headers Additional headers
     * @return HTTP response
     * @throws HttpException on error
     */
    HttpResponse get(const std::string& endpoint,
                    const std::map<std::string, std::string>& headers = {});

    /**
     * @brief Set default headers for all requests
     */
    void setDefaultHeaders(const std::map<std::string, std::string>& headers);

    /**
     * @brief Set request timeout
     */
    void setTimeout(uint32_t timeout_ms);

    /**
     * @brief Enable/disable SSL verification
     */
    void setSSLVerification(bool enable);

private:
    std::unique_ptr<web::http::client::http_client> client_;
    std::string base_url_;
    uint32_t timeout_ms_;
    std::map<std::string, std::string> default_headers_;
    mutable std::mutex mutex_;
    
    // Helper to convert std::map to http_headers
    web::http::http_headers buildHeaders(const std::map<std::string, std::string>& headers);
    
    // Helper to convert http_response to HttpResponse
    HttpResponse convertResponse(const web::http::http_response& response);
};

} // namespace ecoWatt
