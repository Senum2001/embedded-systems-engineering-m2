/**
 * @file ecoWatt_device.hpp
 * @brief Main EcoWatt Device integration header
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

#include "types.hpp"
#include "exceptions.hpp"
#include "config_manager.hpp"
#include "protocol_adapter.hpp"
#include "acquisition_scheduler.hpp"
#include "data_storage.hpp"
#include <memory>
#include <string>
#include <map>

namespace ecoWatt {

/**
 * @brief System status structure
 */
struct SystemStatus {
    bool is_running = false;
    AcquisitionStatistics acquisition_stats;
    StorageStatistics storage_stats;
    AcquisitionConfig acquisition_config;
    TimePoint status_timestamp;
};

/**
 * @brief Current reading data
 */
struct ReadingData {
    double scaled_value;
    std::string unit;
    TimePoint timestamp;
    RegisterValue raw_value;
};

/**
 * @brief Main EcoWatt Device class integrating all components
 */
class EcoWattDevice {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager
     */
    explicit EcoWattDevice(const ConfigManager& config);

    /**
     * @brief Destructor
     */
    ~EcoWattDevice();

    /**
     * @brief Start data acquisition
     */
    void startAcquisition();

    /**
     * @brief Stop data acquisition
     */
    void stopAcquisition();

    /**
     * @brief Check if acquisition is running
     */
    bool isRunning() const { return is_running_; }

    /**
     * @brief Test communication with inverter
     */
    bool testCommunication() const;

    /**
     * @brief Get current readings from all monitored registers
     */
    std::map<std::string, ReadingData> getCurrentReadings() const;

    /**
     * @brief Set export power percentage
     * @param percentage Export power percentage (0-100)
     * @return True if successful
     */
    bool setExportPower(uint8_t percentage);

    /**
     * @brief Get historical data for a register
     * @param register_address Register address
     * @param duration Duration of history to retrieve
     * @return Vector of historical data points
     */
    std::vector<ReadingData> getHistoricalData(RegisterAddress register_address,
                                             Duration duration = Duration(24 * 60 * 60 * 1000)) const;

    /**
     * @brief Export data to file
     * @param filename Output filename
     * @param format Export format ("csv" or "json")
     * @param duration Duration of data to export (0 = all)
     */
    void exportData(const std::string& filename,
                   const std::string& format = "csv",
                   Duration duration = Duration(0)) const;

    /**
     * @brief Get comprehensive system status
     */
    SystemStatus getSystemStatus() const;

    /**
     * @brief Load configuration from file
     * @param config_file Configuration file path
     */
    void loadConfiguration(const std::string& config_file);

    /**
     * @brief Save current configuration to file
     * @param config_file Configuration file path
     */
    void saveConfiguration(const std::string& config_file) const;

    /**
     * @brief Update acquisition configuration at runtime
     */
    void updateAcquisitionConfig(const AcquisitionConfig& config);

    /**
     * @brief Update storage configuration at runtime
     */
    void updateStorageConfig(const StorageConfig& config);

    /**
     * @brief Add or update register configuration
     */
    void setRegisterConfig(RegisterAddress address, const RegisterConfig& config);

    /**
     * @brief Get register configuration
     */
    const RegisterConfig& getRegisterConfig(RegisterAddress address) const;

    /**
     * @brief Get all register configurations
     */
    const std::map<RegisterAddress, RegisterConfig>& getAllRegisterConfigs() const;

private:
    /**
     * @brief Initialize all components
     */
    void initializeComponents();

    /**
     * @brief Setup callbacks between components
     */
    void setupCallbacks();

    /**
     * @brief Sample callback for data storage
     */
    void onSampleAcquired(const AcquisitionSample& sample);

    /**
     * @brief Error callback for logging
     */
    void onAcquisitionError(const std::string& error_message);

    /**
     * @brief Convert acquisition sample to reading data
     */
    ReadingData sampleToReadingData(const AcquisitionSample& sample) const;

    // Configuration
    UniquePtr<ConfigManager> config_manager_;
    
    // Core components
    SharedPtr<ProtocolAdapter> protocol_adapter_;
    SharedPtr<AcquisitionScheduler> acquisition_scheduler_;
    SharedPtr<HybridDataStorage> data_storage_;
    
    // State
    std::atomic<bool> is_running_{false};
    std::atomic<bool> initialized_{false};
    
    // Thread safety
    mutable std::mutex state_mutex_;
};

} // namespace ecoWatt
