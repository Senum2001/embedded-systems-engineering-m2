/**
 * @file logger.hpp
 * @brief Logging system for EcoWatt Device
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

// Prevent Windows macro conflicts
#ifdef ERROR
#undef ERROR
#endif

#include "types.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <string>

namespace ecoWatt {

/**
 * @brief Centralized logging system using spdlog
 */
class Logger {
public:
    /**
     * @brief Initialize the logging system
     * @param config Logging configuration
     */
    static void initialize(const LoggingConfig& config);

    /**
     * @brief Get the logger instance
     */
    static std::shared_ptr<spdlog::logger> get(const std::string& name = "ecoWatt");

    /**
     * @brief Set log level for all loggers
     */
    static void setLevel(LogLevel level);

    /**
     * @brief Flush all loggers
     */
    static void flush();

    /**
     * @brief Shutdown the logging system
     */
    static void shutdown();

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static bool initialized_;
    
    static spdlog::level::level_enum convertLogLevel(LogLevel level);
};

// Convenience macros for logging
#define LOG_TRACE(...) ecoWatt::Logger::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ecoWatt::Logger::get()->debug(__VA_ARGS__)
#define LOG_INFO(...) ecoWatt::Logger::get()->info(__VA_ARGS__)
#define LOG_WARN(...) ecoWatt::Logger::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ecoWatt::Logger::get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ecoWatt::Logger::get()->critical(__VA_ARGS__)

} // namespace ecoWatt
