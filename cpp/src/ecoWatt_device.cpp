/**
 * @file ecoWatt_device.cpp
 * @brief Main EcoWatt Device integration implementation
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#include "ecoWatt_device.hpp"
#include "logger.hpp"
#include "http_client.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace ecoWatt {

// Constructor
EcoWattDevice::EcoWattDevice(const ConfigManager& config) {
    // Create a unique pointer to the config manager
    config_manager_ = std::make_unique<ConfigManager>(config);
    
    LOG_INFO("EcoWatt Device initializing...");
    initializeComponents();
    setupCallbacks();
    
    initialized_ = true;
    LOG_INFO("EcoWatt Device initialized successfully");
}

// Destructor
EcoWattDevice::~EcoWattDevice() {
    if (is_running_.load()) {
        stopAcquisition();
    }
    LOG_INFO("EcoWatt Device destroyed");
}

// Initialize components
void EcoWattDevice::initializeComponents() {
    // Initialize protocol adapter (it will create HTTP client internally)
    protocol_adapter_ = std::make_shared<ProtocolAdapter>(*config_manager_);
    
    // Initialize data storage
    auto storage_config = config_manager_->getStorageConfig();
    data_storage_ = std::make_shared<HybridDataStorage>(storage_config);
    
    // Initialize acquisition scheduler
    acquisition_scheduler_ = std::make_shared<AcquisitionScheduler>(
        protocol_adapter_, 
        *config_manager_
    );
    
    LOG_INFO("All components initialized");
}

// Setup callbacks
void EcoWattDevice::setupCallbacks() {
    // Add sample callback for data storage
    acquisition_scheduler_->addSampleCallback(
        [this](const AcquisitionSample& sample) {
            onSampleAcquired(sample);
        }
    );
    
    // Add error callback for logging
    acquisition_scheduler_->addErrorCallback(
        [this](const std::string& error_message) {
            onAcquisitionError(error_message);
        }
    );
    
    // Setup minimum registers from config
    auto config = config_manager_->getAcquisitionConfig();
    acquisition_scheduler_->setMinimumRegisters(config.minimum_registers);
    
    // Configure registers from config
    auto register_configs = config_manager_->getRegisterConfigs();
    acquisition_scheduler_->configureRegisters(register_configs);
    
    LOG_INFO("Callbacks and configuration setup complete");
}

// Start acquisition
void EcoWattDevice::startAcquisition() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (is_running_.load()) {
        LOG_WARN("EcoWatt Device already running");
        return;
    }
    
    if (!initialized_.load()) {
        throw std::runtime_error("Device not initialized");
    }
    
    // Test communication first
    if (!testCommunication()) {
        throw std::runtime_error("Communication test failed - cannot start acquisition");
    }
    
    // Start acquisition scheduler
    acquisition_scheduler_->startPolling();
    is_running_ = true;
    
    LOG_INFO("EcoWatt Device acquisition started");
}

// Stop acquisition
void EcoWattDevice::stopAcquisition() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!is_running_.load()) {
        LOG_WARN("EcoWatt Device not running");
        return;
    }
    
    // Stop acquisition scheduler
    acquisition_scheduler_->stopPolling();
    is_running_ = false;
    
    LOG_INFO("EcoWatt Device acquisition stopped");
}

// Test communication
bool EcoWattDevice::testCommunication() const {
    try {
        // Try to read a basic register (register 0 - voltage)
        auto sample = acquisition_scheduler_->readSingleRegister(0);
        if (sample) {
            LOG_INFO("Communication test successful - Voltage: {:.2f}V", sample->scaled_value);
            return true;
        } else {
            LOG_ERROR("Communication test failed - no response from register 0");
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Communication test failed: {}", e.what());
        return false;
    }
}

// Get current readings
std::map<std::string, ReadingData> EcoWattDevice::getCurrentReadings() const {
    std::map<std::string, ReadingData> readings;
    
    try {
        // Get all register configurations
        auto register_configs = config_manager_->getRegisterConfigs();
        
        // Read all configured registers
        std::vector<RegisterAddress> addresses;
        for (const auto& pair : register_configs) {
            addresses.push_back(pair.first);
        }
        
        auto samples = acquisition_scheduler_->readMultipleRegisters(addresses);
        
        // Convert to ReadingData
        for (const auto& sample : samples) {
            ReadingData reading = sampleToReadingData(sample);
            readings[sample.register_name] = reading;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get current readings: {}", e.what());
    }
    
    return readings;
}

// Set export power
bool EcoWattDevice::setExportPower(uint8_t percentage) {
    if (percentage > 100) {
        LOG_ERROR("Invalid export power percentage: {}% (must be 0-100)", percentage);
        return false;
    }
    
    try {
        // Register 1 is typically used for export power control
        RegisterAddress power_register = 1;
        RegisterValue power_value = static_cast<RegisterValue>(percentage);
        
        bool success = acquisition_scheduler_->performWriteOperation(power_register, power_value);
        
        if (success) {
            LOG_INFO("Export power set to {}%", percentage);
        } else {
            LOG_ERROR("Failed to set export power to {}%", percentage);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception setting export power: {}", e.what());
        return false;
    }
}

// Get historical data
std::vector<ReadingData> EcoWattDevice::getHistoricalData(RegisterAddress register_address, Duration duration) const {
    std::vector<ReadingData> historical_data;
    
    try {
        // Calculate time range
        auto end_time = std::chrono::system_clock::now();
        auto start_time = end_time - duration;
        
        // Get samples from storage
        auto samples = data_storage_->getHistoricalSamples(register_address, start_time, end_time);
        
        // Convert to ReadingData
        for (const auto& sample : samples) {
            ReadingData reading = sampleToReadingData(sample);
            historical_data.push_back(reading);
        }
        
        LOG_DEBUG("Retrieved {} historical data points for register {}", 
                 historical_data.size(), register_address);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get historical data: {}", e.what());
    }
    
    return historical_data;
}

// Export data
void EcoWattDevice::exportData(const std::string& filename, const std::string& format, Duration duration) const {
    try {
        // Use the storage's built-in export methods
        if (format == "csv") {
            // Calculate time range if duration is specified
            if (duration.count() > 0) {
                auto end_time = std::chrono::system_clock::now();
                auto start_time = end_time - duration;
                data_storage_->exportToCSV(filename, {}, start_time, end_time);
            } else {
                data_storage_->exportToCSV(filename);
            }
        } else if (format == "json") {
            if (duration.count() > 0) {
                auto end_time = std::chrono::system_clock::now();
                auto start_time = end_time - duration;
                data_storage_->exportToJSON(filename, {}, start_time, end_time);
            } else {
                data_storage_->exportToJSON(filename);
            }
        } else {
            throw std::invalid_argument("Unsupported export format: " + format);
        }
        
        LOG_INFO("Exported data to {} in {} format", filename, format);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to export data: {}", e.what());
        throw;
    }
}

// Get system status
SystemStatus EcoWattDevice::getSystemStatus() const {
    SystemStatus status;
    
    status.is_running = is_running_.load();
    status.acquisition_stats = acquisition_scheduler_->getStatistics();
    status.storage_stats = data_storage_->getCombinedStatistics().memory_stats; // Use memory stats
    status.acquisition_config = acquisition_scheduler_->getConfig();
    status.status_timestamp = std::chrono::system_clock::now();
    
    return status;
}

// Load configuration
void EcoWattDevice::loadConfiguration(const std::string& config_file) {
    // Create new ConfigManager with the specified file
    config_manager_ = std::make_unique<ConfigManager>(config_file);
    
    // Reinitialize components with new config
    if (initialized_.load()) {
        bool was_running = is_running_.load();
        
        if (was_running) {
            stopAcquisition();
        }
        
        initializeComponents();
        setupCallbacks();
        
        if (was_running) {
            startAcquisition();
        }
    }
    
    LOG_INFO("Configuration loaded from: {}", config_file);
}

// Save configuration
void EcoWattDevice::saveConfiguration(const std::string& config_file) const {
    config_manager_->saveConfiguration(config_file);
    LOG_INFO("Configuration saved to: {}", config_file);
}

// Update acquisition config
void EcoWattDevice::updateAcquisitionConfig(const AcquisitionConfig& config) {
    config_manager_->updateAcquisitionConfig(config);
    acquisition_scheduler_->setPollingInterval(config.polling_interval);
    acquisition_scheduler_->setMinimumRegisters(config.minimum_registers);
    
    LOG_INFO("Acquisition configuration updated");
}

// Update storage config
void EcoWattDevice::updateStorageConfig(const StorageConfig& config) {
    config_manager_->updateStorageConfig(config);
    // Note: Storage config changes may require restart to take full effect
    
    LOG_INFO("Storage configuration updated");
}

// Set register config
void EcoWattDevice::setRegisterConfig(RegisterAddress address, const RegisterConfig& config) {
    config_manager_->setRegisterConfig(address, config);
    
    // Update scheduler configuration
    auto register_configs = config_manager_->getRegisterConfigs();
    acquisition_scheduler_->configureRegisters(register_configs);
    
    LOG_INFO("Register configuration updated for address: {}", address);
}

// Get register config
const RegisterConfig& EcoWattDevice::getRegisterConfig(RegisterAddress address) const {
    auto register_configs = config_manager_->getRegisterConfigs();
    auto it = register_configs.find(address);
    
    if (it == register_configs.end()) {
        throw std::runtime_error("Register configuration not found for address: " + std::to_string(address));
    }
    
    return it->second;
}

// Get all register configs
const std::map<RegisterAddress, RegisterConfig>& EcoWattDevice::getAllRegisterConfigs() const {
    return config_manager_->getRegisterConfigs();
}

// Sample callback
void EcoWattDevice::onSampleAcquired(const AcquisitionSample& sample) {
    try {
        // Store sample in data storage
        data_storage_->storeSample(sample);
        
        LOG_TRACE("Sample stored: {} = {:.2f} {}", 
                 sample.register_name, sample.scaled_value, sample.unit);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error storing sample: {}", e.what());
    }
}

// Error callback
void EcoWattDevice::onAcquisitionError(const std::string& error_message) {
    LOG_ERROR("Acquisition error: {}", error_message);
    
    // Could implement additional error handling here
    // e.g., retry logic, notifications, etc.
}

// Convert sample to reading data
ReadingData EcoWattDevice::sampleToReadingData(const AcquisitionSample& sample) const {
    ReadingData reading;
    reading.scaled_value = sample.scaled_value;
    reading.unit = sample.unit;
    reading.timestamp = sample.timestamp;
    reading.raw_value = sample.raw_value;
    
    return reading;
}

} // namespace ecoWatt
