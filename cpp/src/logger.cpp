/**
 * @file logger.cpp
 * @brief Implementation of logging system
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#include "logger.hpp"
#include <iostream>

namespace ecoWatt {

std::shared_ptr<spdlog::logger> Logger::logger_;
bool Logger::initialized_ = false;

void Logger::initialize(const LoggingConfig& config) {
    if (initialized_) {
        return;
    }

    try {
        // Create sinks
        std::vector<spdlog::sink_ptr> sinks;

        // Console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(convertLogLevel(config.console_level));
        sinks.push_back(console_sink);

        // File sink (rotating)
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            config.log_file, 
            config.max_file_size_mb * 1024 * 1024, 
            config.max_files
        );
        file_sink->set_level(convertLogLevel(config.file_level));
        sinks.push_back(file_sink);

        // Create logger
        logger_ = std::make_shared<spdlog::logger>("ecoWatt", sinks.begin(), sinks.end());
        logger_->set_level(spdlog::level::trace); // Let sinks filter
        logger_->flush_on(spdlog::level::warn);

        // Set pattern
        logger_->set_pattern(config.format);

        // Register as default logger
        spdlog::set_default_logger(logger_);

        initialized_ = true;
        
        LOG_INFO("Logging system initialized");
        LOG_INFO("Console level: {}, File level: {}", 
                to_string(config.console_level), 
                to_string(config.file_level));
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        throw;
    }
}

std::shared_ptr<spdlog::logger> Logger::get(const std::string& name) {
    if (!initialized_) {
        // Initialize with default config if not already done
        LoggingConfig default_config;
        initialize(default_config);
    }
    
    if (name == "ecoWatt" || name.empty()) {
        return logger_;
    }
    
    // Return named logger or create it
    auto named_logger = spdlog::get(name);
    if (!named_logger) {
        named_logger = logger_->clone(name);
        spdlog::register_logger(named_logger);
    }
    
    return named_logger;
}

void Logger::setLevel(LogLevel level) {
    if (logger_) {
        logger_->set_level(convertLogLevel(level));
    }
}

void Logger::flush() {
    if (logger_) {
        logger_->flush();
    }
}

void Logger::shutdown() {
    if (initialized_) {
        LOG_INFO("Shutting down logging system");
        flush();
        spdlog::shutdown();
        initialized_ = false;
    }
}

spdlog::level::level_enum Logger::convertLogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return spdlog::level::trace;
        case LogLevel::DEBUG: return spdlog::level::debug;
        case LogLevel::INFO: return spdlog::level::info;
        case LogLevel::WARN: return spdlog::level::warn;
        case LogLevel::ERROR: return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        default: return spdlog::level::info;
    }
}

} // namespace ecoWatt
