/**
 * @file config_manager.cpp
 * @brief Implementation of configuration management
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#include "config_manager.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace ecoWatt {

ConfigManager::ConfigManager(const std::string& config_file, const std::string& env_file) {
    // Load environment variables first (they can override config file values)
    loadEnvironmentVariables(env_file);
    
    // Load JSON configuration
    loadJsonConfiguration(config_file);
    
    // Validate configuration
    validateConfiguration();
    
    LOG_INFO("Configuration loaded successfully");
}

void ConfigManager::loadEnvironmentVariables(const std::string& env_file) {
    std::ifstream file(env_file);
    if (!file.is_open()) {
        LOG_WARN("Environment file '{}' not found, using defaults", env_file);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse KEY=VALUE format
        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, equals_pos);
        std::string value = line.substr(equals_pos + 1);
        
        // Trim whitespace
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        env_vars_[key] = value;
    }

    LOG_DEBUG("Loaded {} environment variables from '{}'", env_vars_.size(), env_file);
}

void ConfigManager::loadJsonConfiguration(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        throw ConfigException("Cannot open configuration file: " + config_file);
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& e) {
        throw ConfigException("Invalid JSON in configuration file: " + std::string(e.what()));
    }

    // Application info
    if (json.contains("application")) {
        const auto& app = json["application"];
        app_name_ = app.value("name", "EcoWatt Device");
        app_version_ = app.value("version", "2.0.0");
        app_description_ = app.value("description", "Inverter SIM Integration");
    }

    // Modbus configuration
    if (json.contains("modbus")) {
        const auto& modbus = json["modbus"];
        modbus_config_.slave_address = modbus.value("slave_address", 17);
        modbus_config_.timeout = Duration(modbus.value("timeout_ms", 5000));
        modbus_config_.max_retries = modbus.value("max_retries", 3);
        modbus_config_.retry_delay = Duration(modbus.value("retry_delay_ms", 1000));
    }

    // Override with environment variables
    if (env_vars_.count("DEFAULT_SLAVE_ADDRESS")) {
        modbus_config_.slave_address = std::stoi(env_vars_["DEFAULT_SLAVE_ADDRESS"]);
    }
    if (env_vars_.count("MAX_RETRIES")) {
        modbus_config_.max_retries = std::stoi(env_vars_["MAX_RETRIES"]);
    }
    if (env_vars_.count("REQUEST_TIMEOUT_MS")) {
        modbus_config_.timeout = Duration(std::stoi(env_vars_["REQUEST_TIMEOUT_MS"]));
    }
    if (env_vars_.count("RETRY_DELAY_MS")) {
        modbus_config_.retry_delay = Duration(std::stoi(env_vars_["RETRY_DELAY_MS"]));
    }

    // Acquisition configuration
    if (json.contains("acquisition")) {
        const auto& acq = json["acquisition"];
        acquisition_config_.polling_interval = Duration(acq.value("polling_interval_ms", 10000));
        acquisition_config_.max_samples_per_register = acq.value("max_samples_per_register", 1000);
        acquisition_config_.enable_background_polling = acq.value("enable_background_polling", true);
        
        if (acq.contains("minimum_registers")) {
            acquisition_config_.minimum_registers.clear();
            for (const auto& reg : acq["minimum_registers"]) {
                acquisition_config_.minimum_registers.push_back(reg.get<RegisterAddress>());
            }
        }
    }

    // Storage configuration
    if (json.contains("storage")) {
        const auto& storage = json["storage"];
        storage_config_.memory_retention_samples = storage.value("memory_retention_samples", 1000);
        storage_config_.enable_persistent_storage = storage.value("enable_persistent_storage", true);
        storage_config_.cleanup_interval = Duration(storage.value("cleanup_interval_hours", 24) * 60 * 60 * 1000);
        storage_config_.data_retention_days = storage.value("data_retention_days", 30);
    }

    // Override database path from environment
    if (env_vars_.count("DATABASE_PATH")) {
        storage_config_.database_path = env_vars_["DATABASE_PATH"];
    }

    // API configuration
    if (json.contains("api")) {
        const auto& api = json["api"];
        if (api.contains("endpoints")) {
            api_config_.read_endpoint = api["endpoints"].value("read", "/api/inverter/read");
            api_config_.write_endpoint = api["endpoints"].value("write", "/api/inverter/write");
        }
        if (api.contains("headers")) {
            api_config_.content_type = api["headers"].value("content_type", "application/json");
            api_config_.accept = api["headers"].value("accept", "*/*");
        }
    }

    // Override API settings from environment
    if (env_vars_.count("INVERTER_API_KEY")) {
        api_config_.api_key = env_vars_["INVERTER_API_KEY"];
    }
    if (env_vars_.count("INVERTER_API_BASE_URL")) {
        api_config_.base_url = env_vars_["INVERTER_API_BASE_URL"];
    }

    // Logging configuration
    if (json.contains("logging")) {
        const auto& logging = json["logging"];
        logging_config_.console_level = log_level_from_string(logging.value("console_level", "INFO"));
        logging_config_.file_level = log_level_from_string(logging.value("file_level", "DEBUG"));
        logging_config_.max_file_size_mb = logging.value("max_file_size_mb", 10);
        logging_config_.max_files = logging.value("max_files", 5);
        logging_config_.format = logging.value("format", "[%Y-%m-%d %H:%M:%S] [%l] %v");
    }

    // Override log settings from environment
    if (env_vars_.count("LOG_LEVEL")) {
        logging_config_.console_level = log_level_from_string(env_vars_["LOG_LEVEL"]);
    }
    if (env_vars_.count("LOG_FILE")) {
        logging_config_.log_file = env_vars_["LOG_FILE"];
    }

    // Register configurations
    parseRegisterConfigs(json);
}

void ConfigManager::parseRegisterConfigs(const nlohmann::json& json) {
    if (!json.contains("registers")) {
        throw ConfigException("No register configurations found in config file");
    }

    for (const auto& [key, reg_json] : json["registers"].items()) {
        RegisterAddress address = std::stoi(key);
        
        RegisterConfig config;
        config.address = address;
        config.name = reg_json.value("name", "Unknown");
        config.unit = reg_json.value("unit", "");
        config.gain = reg_json.value("gain", 1.0);
        config.access = access_from_string(reg_json.value("access", "Read"));
        config.description = reg_json.value("description", "");
        
        register_configs_[address] = config;
    }

    LOG_DEBUG("Loaded {} register configurations", register_configs_.size());
}

const RegisterConfig& ConfigManager::getRegisterConfig(RegisterAddress address) const {
    auto it = register_configs_.find(address);
    if (it == register_configs_.end()) {
        throw ConfigException("Register " + std::to_string(address) + " not configured");
    }
    return it->second;
}

bool ConfigManager::hasRegister(RegisterAddress address) const {
    return register_configs_.find(address) != register_configs_.end();
}

void ConfigManager::saveConfiguration(const std::string& config_file) const {
    nlohmann::json json;
    
    // Application info
    json["application"]["name"] = app_name_;
    json["application"]["version"] = app_version_;
    json["application"]["description"] = app_description_;
    
    // Modbus config
    json["modbus"]["slave_address"] = modbus_config_.slave_address;
    json["modbus"]["timeout_ms"] = modbus_config_.timeout.count();
    json["modbus"]["max_retries"] = modbus_config_.max_retries;
    json["modbus"]["retry_delay_ms"] = modbus_config_.retry_delay.count();
    
    // Acquisition config
    json["acquisition"]["polling_interval_ms"] = acquisition_config_.polling_interval.count();
    json["acquisition"]["max_samples_per_register"] = acquisition_config_.max_samples_per_register;
    json["acquisition"]["enable_background_polling"] = acquisition_config_.enable_background_polling;
    json["acquisition"]["minimum_registers"] = acquisition_config_.minimum_registers;
    
    // Storage config
    json["storage"]["memory_retention_samples"] = storage_config_.memory_retention_samples;
    json["storage"]["enable_persistent_storage"] = storage_config_.enable_persistent_storage;
    json["storage"]["cleanup_interval_hours"] = storage_config_.cleanup_interval.count() / (60 * 60 * 1000);
    json["storage"]["data_retention_days"] = storage_config_.data_retention_days;
    
    // API config
    json["api"]["endpoints"]["read"] = api_config_.read_endpoint;
    json["api"]["endpoints"]["write"] = api_config_.write_endpoint;
    json["api"]["headers"]["content_type"] = api_config_.content_type;
    json["api"]["headers"]["accept"] = api_config_.accept;
    
    // Logging config
    json["logging"]["console_level"] = to_string(logging_config_.console_level);
    json["logging"]["file_level"] = to_string(logging_config_.file_level);
    json["logging"]["max_file_size_mb"] = logging_config_.max_file_size_mb;
    json["logging"]["max_files"] = logging_config_.max_files;
    json["logging"]["format"] = logging_config_.format;
    
    // Register configs
    for (const auto& [address, config] : register_configs_) {
        auto& reg_json = json["registers"][std::to_string(address)];
        reg_json["name"] = config.name;
        reg_json["unit"] = config.unit;
        reg_json["gain"] = config.gain;
        reg_json["access"] = to_string(config.access);
        reg_json["description"] = config.description;
    }
    
    std::ofstream file(config_file);
    if (!file.is_open()) {
        throw ConfigException("Cannot write configuration file: " + config_file);
    }
    
    file << json.dump(2);
    LOG_INFO("Configuration saved to '{}'", config_file);
}

void ConfigManager::updateAcquisitionConfig(const AcquisitionConfig& config) {
    acquisition_config_ = config;
    LOG_INFO("Acquisition configuration updated");
}

void ConfigManager::updateStorageConfig(const StorageConfig& config) {
    storage_config_ = config;
    LOG_INFO("Storage configuration updated");
}

void ConfigManager::updateLoggingConfig(const LoggingConfig& config) {
    logging_config_ = config;
    LOG_INFO("Logging configuration updated");
}

void ConfigManager::setRegisterConfig(RegisterAddress address, const RegisterConfig& config) {
    register_configs_[address] = config;
    LOG_DEBUG("Register {} configuration updated", address);
}

void ConfigManager::removeRegisterConfig(RegisterAddress address) {
    register_configs_.erase(address);
    LOG_DEBUG("Register {} configuration removed", address);
}

void ConfigManager::validateConfiguration() const {
    // Validate API key
    if (api_config_.api_key.empty()) {
        throw ConfigException("API key is required (set INVERTER_API_KEY environment variable)");
    }
    
    // Validate base URL
    if (api_config_.base_url.empty()) {
        throw ConfigException("API base URL is required");
    }
    
    // Validate minimum registers exist
    for (RegisterAddress addr : acquisition_config_.minimum_registers) {
        if (!hasRegister(addr)) {
            throw ConfigException("Minimum register " + std::to_string(addr) + " is not configured");
        }
    }
    
    // Validate polling interval
    if (acquisition_config_.polling_interval.count() < 1000) {
        throw ConfigException("Polling interval must be at least 1000ms");
    }
    
    // Validate timeouts
    if (modbus_config_.timeout.count() < 1000) {
        throw ConfigException("Timeout must be at least 1000ms");
    }
    
    LOG_DEBUG("Configuration validation passed");
}

std::string ConfigManager::getString(const std::string& key, const std::string& default_value) const {
    // First check environment variables
    auto env_it = env_vars_.find(key);
    if (env_it != env_vars_.end()) {
        return env_it->second;
    }
    
    // Then check JSON config using dot notation (e.g., "api.base_url")
    try {
        nlohmann::json config_copy = config_;
        std::string current_key = key;
        
        size_t dot_pos = current_key.find('.');
        while (dot_pos != std::string::npos) {
            std::string section = current_key.substr(0, dot_pos);
            current_key = current_key.substr(dot_pos + 1);
            
            if (config_copy.contains(section)) {
                config_copy = config_copy[section];
            } else {
                return default_value;
            }
            
            dot_pos = current_key.find('.');
        }
        
        if (config_copy.contains(current_key) && config_copy[current_key].is_string()) {
            return config_copy[current_key].get<std::string>();
        }
    } catch (const std::exception&) {
        // Fall through to default
    }
    
    return default_value;
}

int ConfigManager::getInt(const std::string& key, int default_value) const {
    // First check environment variables
    auto env_it = env_vars_.find(key);
    if (env_it != env_vars_.end()) {
        try {
            return std::stoi(env_it->second);
        } catch (const std::exception&) {
            // Fall through to JSON or default
        }
    }
    
    // Then check JSON config
    try {
        nlohmann::json config_copy = config_;
        std::string current_key = key;
        
        size_t dot_pos = current_key.find('.');
        while (dot_pos != std::string::npos) {
            std::string section = current_key.substr(0, dot_pos);
            current_key = current_key.substr(dot_pos + 1);
            
            if (config_copy.contains(section)) {
                config_copy = config_copy[section];
            } else {
                return default_value;
            }
            
            dot_pos = current_key.find('.');
        }
        
        if (config_copy.contains(current_key) && config_copy[current_key].is_number_integer()) {
            return config_copy[current_key].get<int>();
        }
    } catch (const std::exception&) {
        // Fall through to default
    }
    
    return default_value;
}

bool ConfigManager::getBool(const std::string& key, bool default_value) const {
    // First check environment variables
    auto env_it = env_vars_.find(key);
    if (env_it != env_vars_.end()) {
        std::string value = env_it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            return true;
        }
        if (value == "false" || value == "0" || value == "no" || value == "off") {
            return false;
        }
    }
    
    // Then check JSON config
    try {
        nlohmann::json config_copy = config_;
        std::string current_key = key;
        
        size_t dot_pos = current_key.find('.');
        while (dot_pos != std::string::npos) {
            std::string section = current_key.substr(0, dot_pos);
            current_key = current_key.substr(dot_pos + 1);
            
            if (config_copy.contains(section)) {
                config_copy = config_copy[section];
            } else {
                return default_value;
            }
            
            dot_pos = current_key.find('.');
        }
        
        if (config_copy.contains(current_key) && config_copy[current_key].is_boolean()) {
            return config_copy[current_key].get<bool>();
        }
    } catch (const std::exception&) {
        // Fall through to default
    }
    
    return default_value;
}

double ConfigManager::getDouble(const std::string& key, double default_value) const {
    // First check environment variables
    auto env_it = env_vars_.find(key);
    if (env_it != env_vars_.end()) {
        try {
            return std::stod(env_it->second);
        } catch (const std::exception&) {
            // Fall through to JSON or default
        }
    }
    
    // Then check JSON config
    try {
        nlohmann::json config_copy = config_;
        std::string current_key = key;
        
        size_t dot_pos = current_key.find('.');
        while (dot_pos != std::string::npos) {
            std::string section = current_key.substr(0, dot_pos);
            current_key = current_key.substr(dot_pos + 1);
            
            if (config_copy.contains(section)) {
                config_copy = config_copy[section];
            } else {
                return default_value;
            }
            
            dot_pos = current_key.find('.');
        }
        
        if (config_copy.contains(current_key) && config_copy[current_key].is_number()) {
            return config_copy[current_key].get<double>();
        }
    } catch (const std::exception&) {
        // Fall through to default
    }
    
    return default_value;
}

nlohmann::json ConfigManager::getArray(const std::string& key) const {
    try {
        nlohmann::json config_copy = config_;
        std::string current_key = key;
        
        size_t dot_pos = current_key.find('.');
        while (dot_pos != std::string::npos) {
            std::string section = current_key.substr(0, dot_pos);
            current_key = current_key.substr(dot_pos + 1);
            
            if (config_copy.contains(section)) {
                config_copy = config_copy[section];
            } else {
                return nlohmann::json::array();
            }
            
            dot_pos = current_key.find('.');
        }
        
        if (config_copy.contains(current_key) && config_copy[current_key].is_array()) {
            return config_copy[current_key];
        }
    } catch (const std::exception&) {
        // Fall through to empty array
    }
    
    return nlohmann::json::array();
}

} // namespace ecoWatt
