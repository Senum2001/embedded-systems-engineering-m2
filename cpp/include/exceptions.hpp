/**
 * @file exceptions.hpp
 * @brief Custom exception classes for EcoWatt Device
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

#include <stdexcept>
#include <string>

namespace ecoWatt {

/**
 * @brief Base exception class for EcoWatt Device
 */
class EcoWattException : public std::runtime_error {
public:
    explicit EcoWattException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception for Modbus protocol errors
 */
class ModbusException : public EcoWattException {
public:
    explicit ModbusException(const std::string& message)
        : EcoWattException("Modbus Error: " + message) {}
        
    ModbusException(uint8_t error_code, const std::string& description)
        : EcoWattException("Modbus Error (0x" + std::to_string(error_code) + "): " + description) {}
};

/**
 * @brief Exception for HTTP communication errors
 */
class HttpException : public EcoWattException {
public:
    explicit HttpException(const std::string& message)
        : EcoWattException("HTTP Error: " + message) {}
        
    HttpException(long response_code, const std::string& message)
        : EcoWattException("HTTP Error (" + std::to_string(response_code) + "): " + message) {}
};

/**
 * @brief Exception for configuration errors
 */
class ConfigException : public EcoWattException {
public:
    explicit ConfigException(const std::string& message)
        : EcoWattException("Configuration Error: " + message) {}
};

/**
 * @brief Exception for data storage errors
 */
class StorageException : public EcoWattException {
public:
    explicit StorageException(const std::string& message)
        : EcoWattException("Storage Error: " + message) {}
};

/**
 * @brief Exception for acquisition errors
 */
class AcquisitionException : public EcoWattException {
public:
    explicit AcquisitionException(const std::string& message)
        : EcoWattException("Acquisition Error: " + message) {}
};

/**
 * @brief Exception for timeout errors
 */
class TimeoutException : public EcoWattException {
public:
    explicit TimeoutException(const std::string& message)
        : EcoWattException("Timeout Error: " + message) {}
};

/**
 * @brief Exception for validation errors
 */
class ValidationException : public EcoWattException {
public:
    explicit ValidationException(const std::string& message)
        : EcoWattException("Validation Error: " + message) {}
};

} // namespace ecoWatt
