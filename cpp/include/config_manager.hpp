/**
 * @file config_manager.hpp
 * @brief Configuration management for EcoWatt Device
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

#include "types.hpp"
#include "exceptions.hpp"
#include <nlohmann/json.hpp>
#include <map>
#include <string>

namespace ecoWatt {

/**
 * @brief Manages application configuration from environment variables and JSON files
 */
class ConfigManager {
public:
    /**
     * @brief Constructor - loads configuration from files
     * @param config_file Path to JSON configuration file
     * @param env_file Path to environment file (.env)
     */
    ConfigManager(const std::string& config_file = "config.json", 
                  const std::string& env_file = ".env");

    /**
     * @brief Get Modbus configuration
     */
    const ModbusConfig& getModbusConfig() const { return modbus_config_; }

    /**
     * @brief Get acquisition configuration
     */
    const AcquisitionConfig& getAcquisitionConfig() const { return acquisition_config_; }

    /**
     * @brief Get storage configuration
     */
    const StorageConfig& getStorageConfig() const { return storage_config_; }

    /**
     * @brief Get API configuration
     */
    const ApiConfig& getApiConfig() const { return api_config_; }

    /**
     * @brief Get logging configuration
     */
    const LoggingConfig& getLoggingConfig() const { return logging_config_; }

    /**
     * @brief Get register configurations
     */
    const std::map<RegisterAddress, RegisterConfig>& getRegisterConfigs() const { 
        return register_configs_; 
    }

    /**
     * @brief Get register configuration by address
     * @param address Register address
     * @throws ConfigException if register not found
     */
    const RegisterConfig& getRegisterConfig(RegisterAddress address) const;

    /**
     * @brief Check if register exists
     * @param address Register address
     */
    bool hasRegister(RegisterAddress address) const;

    /**
     * @brief Get application information
     */
    const std::string& getAppName() const { return app_name_; }
    const std::string& getAppVersion() const { return app_version_; }
    const std::string& getAppDescription() const { return app_description_; }

    /**
     * @brief Save current configuration to file
     * @param config_file Path to save configuration
     */
    void saveConfiguration(const std::string& config_file) const;

    /**
     * @brief Update configuration at runtime
     */
    void updateAcquisitionConfig(const AcquisitionConfig& config);
    void updateStorageConfig(const StorageConfig& config);
    void updateLoggingConfig(const LoggingConfig& config);

    /**
     * @brief Add or update register configuration
     */
    void setRegisterConfig(RegisterAddress address, const RegisterConfig& config);

    /**
     * @brief Remove register configuration
     */
    void removeRegisterConfig(RegisterAddress address);

    /**
     * @brief Validate configuration consistency
     * @throws ConfigException if configuration is invalid
     */
    void validateConfiguration() const;

    /**
     * @brief Helper methods to get configuration values with defaults
     */
    std::string getString(const std::string& key, const std::string& default_value = "") const;
    int getInt(const std::string& key, int default_value = 0) const;
    bool getBool(const std::string& key, bool default_value = false) const;
    double getDouble(const std::string& key, double default_value = 0.0) const;
    nlohmann::json getArray(const std::string& key) const;

private:
    void loadEnvironmentVariables(const std::string& env_file);
    void loadJsonConfiguration(const std::string& config_file);
    void parseRegisterConfigs(const nlohmann::json& json);
    
    // Configuration sections
    ModbusConfig modbus_config_;
    AcquisitionConfig acquisition_config_;
    StorageConfig storage_config_;
    ApiConfig api_config_;
    LoggingConfig logging_config_;
    
    // Register configurations
    std::map<RegisterAddress, RegisterConfig> register_configs_;
    
    // Application information
    std::string app_name_;
    std::string app_version_;
    std::string app_description_;
    
    // Environment variables
    std::map<std::string, std::string> env_vars_;
    
    // Full JSON configuration for complex queries
    nlohmann::json config_;
};

} // namespace ecoWatt
