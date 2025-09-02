/**
 * @file types.hpp
 * @brief Common type definitions for EcoWatt Device
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <map>

namespace ecoWatt {

// Type aliases for clarity
using RegisterAddress = uint16_t;
using RegisterValue = uint16_t;
using SlaveAddress = uint8_t;
using FunctionCode = uint8_t;
using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::milliseconds;

// Modbus function codes
enum class ModbusFunction : FunctionCode {
    READ_HOLDING_REGISTERS = 0x03,
    WRITE_SINGLE_REGISTER = 0x06
};

// Register access types
enum class AccessType {
    READ_ONLY,
    WRITE_ONLY,
    READ_WRITE
};

// Log levels
#ifdef ERROR
#undef ERROR  // Undefine Windows ERROR macro if present
#endif
enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

// Register configuration structure
struct RegisterConfig {
    RegisterAddress address;
    std::string name;
    std::string unit;
    double gain;
    AccessType access;
    std::string description;
    
    RegisterConfig() = default;
    
    RegisterConfig(RegisterAddress addr, const std::string& n, const std::string& u, 
                   double g, AccessType a, const std::string& desc)
        : address(addr), name(n), unit(u), gain(g), access(a), description(desc) {}
};

// Acquisition sample structure
struct AcquisitionSample {
    TimePoint timestamp;
    RegisterAddress register_address;
    std::string register_name;
    RegisterValue raw_value;
    double scaled_value;
    std::string unit;
    
    AcquisitionSample() = default;
    
    AcquisitionSample(TimePoint ts, RegisterAddress addr, const std::string& name,
                     RegisterValue raw, double scaled, const std::string& u)
        : timestamp(ts), register_address(addr), register_name(name),
          raw_value(raw), scaled_value(scaled), unit(u) {}
};

// Modbus response structure
struct ModbusResponse {
    SlaveAddress slave_address;
    FunctionCode function_code;
    std::vector<uint8_t> data;
    bool is_error = false;
    uint8_t error_code = 0;
    
    ModbusResponse() = default;
    
    ModbusResponse(SlaveAddress addr, FunctionCode func, const std::vector<uint8_t>& d)
        : slave_address(addr), function_code(func), data(d) {}
};

// Statistics structures
struct AcquisitionStatistics {
    uint64_t total_polls = 0;
    uint64_t successful_polls = 0;
    uint64_t failed_polls = 0;
    TimePoint last_poll_time;
    std::string last_error;
    
    double success_rate() const {
        return total_polls > 0 ? static_cast<double>(successful_polls) / total_polls : 0.0;
    }
};

struct StorageStatistics {
    uint64_t total_samples = 0;
    std::map<RegisterAddress, uint64_t> samples_by_register;
    TimePoint oldest_sample_time;
    TimePoint newest_sample_time;
    uint64_t storage_size_bytes = 0;
};

// Configuration structures
struct ModbusConfig {
    SlaveAddress slave_address = 17;
    Duration timeout = Duration(5000);
    uint32_t max_retries = 3;
    Duration retry_delay = Duration(1000);
};

struct AcquisitionConfig {
    Duration polling_interval = Duration(10000);
    uint32_t max_samples_per_register = 1000;
    std::vector<RegisterAddress> minimum_registers = {0, 1};
    bool enable_background_polling = true;
};

struct StorageConfig {
    uint32_t memory_retention_samples = 1000;
    bool enable_persistent_storage = true;
    Duration cleanup_interval = Duration(24 * 60 * 60 * 1000); // 24 hours
    uint32_t data_retention_days = 30;
    std::string database_path = "ecoWatt_milestone2.db";
};

struct ApiConfig {
    std::string base_url = "http://20.15.114.131:8080";
    std::string api_key;
    std::string read_endpoint = "/api/inverter/read";
    std::string write_endpoint = "/api/inverter/write";
    std::string content_type = "application/json";
    std::string accept = "*/*";
};

struct LoggingConfig {
    LogLevel console_level = LogLevel::INFO;
    LogLevel file_level = LogLevel::DEBUG;
    std::string log_file = "ecoWatt_milestone2.log";
    uint32_t max_file_size_mb = 10;
    uint32_t max_files = 5;
    std::string format = "[%Y-%m-%d %H:%M:%S] [%l] %v";
};

// Smart pointer aliases
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T, typename... Args>
constexpr UniquePtr<T> make_unique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
constexpr SharedPtr<T> make_shared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// Helper functions
inline std::string to_string(AccessType access) {
    switch (access) {
        case AccessType::READ_ONLY: return "Read";
        case AccessType::WRITE_ONLY: return "Write";
        case AccessType::READ_WRITE: return "Read/Write";
        default: return "Unknown";
    }
}

inline AccessType access_from_string(const std::string& str) {
    if (str == "Read") return AccessType::READ_ONLY;
    if (str == "Write") return AccessType::WRITE_ONLY;
    if (str == "Read/Write") return AccessType::READ_WRITE;
    return AccessType::READ_ONLY;
}

inline std::string to_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

inline LogLevel log_level_from_string(const std::string& str) {
    if (str == "TRACE") return LogLevel::TRACE;
    if (str == "DEBUG") return LogLevel::DEBUG;
    if (str == "INFO") return LogLevel::INFO;
    if (str == "WARN") return LogLevel::WARN;
    if (str == "ERROR") return LogLevel::ERROR;
    if (str == "CRITICAL") return LogLevel::CRITICAL;
    return LogLevel::INFO;
}

} // namespace ecoWatt
