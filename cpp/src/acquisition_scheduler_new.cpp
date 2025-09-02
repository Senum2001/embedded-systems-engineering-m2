/**
 * @file acquisition_scheduler.cpp
 * @brief Data acquisition scheduler implementation
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#include "acquisition_scheduler.hpp"
#include "logging.hpp"
#include <chrono>
#include <algorithm>

namespace ecoWatt {

// Constructor
AcquisitionScheduler::AcquisitionScheduler(SharedPtr<ProtocolAdapter> protocol_adapter,
                                         const ConfigManager& config)
    : protocol_adapter_(protocol_adapter), max_buffer_size_(10000) {
    
    // Get configuration
    config_ = config.getAcquisitionConfig();
    minimum_registers_ = config_.minimum_registers;
    
    LOG_INFO("AcquisitionScheduler initialized with interval: {}ms", config_.polling_interval.count());
}

// Destructor
AcquisitionScheduler::~AcquisitionScheduler() {
    stopPolling();
    LOG_INFO("AcquisitionScheduler destroyed");
}

// Start polling
void AcquisitionScheduler::startPolling() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    if (polling_active_.load()) {
        LOG_WARN("AcquisitionScheduler already polling");
        return;
    }
    
    stop_requested_ = false;
    polling_active_ = true;
    
    // Start background thread
    polling_thread_ = std::make_unique<std::thread>(&AcquisitionScheduler::pollingLoop, this);
    
    LOG_INFO("AcquisitionScheduler started polling");
}

// Stop polling
void AcquisitionScheduler::stopPolling() {
    stop_requested_ = true;
    polling_active_ = false;
    
    if (polling_thread_ && polling_thread_->joinable()) {
        polling_thread_->join();
    }
    
    LOG_INFO("AcquisitionScheduler stopped polling");
}

// Set polling interval
void AcquisitionScheduler::setPollingInterval(Duration interval) {
    config_.polling_interval = interval;
    LOG_INFO("Updated polling interval to: {}ms", interval.count());
}

// Set minimum registers
void AcquisitionScheduler::setMinimumRegisters(const std::vector<RegisterAddress>& registers) {
    minimum_registers_ = registers;
    config_.minimum_registers = registers;
}

// Configure registers
void AcquisitionScheduler::configureRegisters(const std::map<RegisterAddress, RegisterConfig>& register_configs) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    register_configs_ = register_configs;
}

// Add sample callback
void AcquisitionScheduler::addSampleCallback(SampleCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    sample_callbacks_.push_back(callback);
}

// Add error callback
void AcquisitionScheduler::addErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    error_callbacks_.push_back(callback);
}

// Read single register
UniquePtr<AcquisitionSample> AcquisitionScheduler::readSingleRegister(RegisterAddress address) {
    try {
        auto response = protocol_adapter_->readHoldingRegisters(address, 1);
        if (response.is_error || response.data.empty()) {
            return nullptr;
        }
        
        RegisterValue value = (response.data[0] << 8) | response.data[1];
        
        auto it = register_configs_.find(address);
        std::string name = (it != register_configs_.end()) ? it->second.name : "Unknown";
        std::string unit = (it != register_configs_.end()) ? it->second.unit : "";
        double gain = (it != register_configs_.end()) ? it->second.gain : 1.0;
        
        // Per API docs, 'gain' is a scaling divisor
        double scaled = (gain != 0.0) ? static_cast<double>(value) / gain
                                       : static_cast<double>(value);

        auto sample = std::make_unique<AcquisitionSample>(
            std::chrono::system_clock::now(),
            address,
            name,
            value,
            scaled,
            unit
        );
        
        return sample;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to read register {}: {}", address, e.what());
        return nullptr;
    }
}

// Read multiple registers
std::vector<AcquisitionSample> AcquisitionScheduler::readMultipleRegisters(const std::vector<RegisterAddress>& addresses) {
    std::vector<AcquisitionSample> samples;
    
    for (auto address : addresses) {
        auto sample = readSingleRegister(address);
        if (sample) {
            samples.push_back(*sample);
        }
    }
    
    return samples;
}

// Perform write operation
bool AcquisitionScheduler::performWriteOperation(RegisterAddress register_address, RegisterValue value) {
    try {
        auto response = protocol_adapter_->writeSingleRegister(register_address, value);
        return !response.is_error;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to write register {}: {}", register_address, e.what());
        return false;
    }
}

// Get recent samples
std::vector<AcquisitionSample> AcquisitionScheduler::getRecentSamples(size_t count) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    std::vector<AcquisitionSample> result;
    size_t num_samples = std::min(count, sample_buffer_.size());
    
    auto start_it = sample_buffer_.end() - num_samples;
    result.assign(start_it, sample_buffer_.end());
    
    return result;
}

// Get samples by register
std::vector<AcquisitionSample> AcquisitionScheduler::getSamplesByRegister(RegisterAddress register_address, size_t count) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    std::vector<AcquisitionSample> result;
    
    for (auto it = sample_buffer_.rbegin(); it != sample_buffer_.rend() && result.size() < count; ++it) {
        if (it->register_address == register_address) {
            result.push_back(*it);
        }
    }
    
    std::reverse(result.begin(), result.end());
    return result;
}

// Reset statistics
void AcquisitionScheduler::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    statistics_ = AcquisitionStatistics{};
}

// Main polling loop
void AcquisitionScheduler::pollingLoop() {
    LOG_INFO("Polling loop started");
    
    while (!stop_requested_.load()) {
        try {
            performPollCycle();
        } catch (const std::exception& e) {
            LOG_ERROR("Error in polling cycle: {}", e.what());
            notifyError(e.what());
        }
        
        // Wait for next poll interval
        std::this_thread::sleep_for(config_.polling_interval);
    }
    
    LOG_INFO("Polling loop stopped");
}

// Perform poll cycle
void AcquisitionScheduler::performPollCycle() {
    std::vector<RegisterAddress> addresses_to_read;
    
    // Collect all configured register addresses
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        for (const auto& pair : register_configs_) {
            addresses_to_read.push_back(pair.first);
        }
    }
    
    // Add minimum registers if not already included
    for (auto addr : minimum_registers_) {
        if (std::find(addresses_to_read.begin(), addresses_to_read.end(), addr) == addresses_to_read.end()) {
            addresses_to_read.push_back(addr);
        }
    }
    
    // Read all registers
    auto samples = readMultipleRegisters(addresses_to_read);
    
    // Store samples and notify callbacks
    for (const auto& sample : samples) {
        storeSample(sample);
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.total_polls++;
        statistics_.last_poll_time = std::chrono::system_clock::now();
        
        if (!samples.empty()) {
            statistics_.successful_polls++;
        } else {
            statistics_.failed_polls++;
            statistics_.last_error = "No samples acquired";
        }
    }
}

// Store sample
void AcquisitionScheduler::storeSample(const AcquisitionSample& sample) {
    // Store in buffer
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        sample_buffer_.push_back(sample);
        
        // Keep buffer size under limit
        while (sample_buffer_.size() > max_buffer_size_) {
            sample_buffer_.pop_front();
        }
    }
    
    // Notify callbacks
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        for (const auto& callback : sample_callbacks_) {
            try {
                callback(sample);
            } catch (const std::exception& e) {
                LOG_ERROR("Error in sample callback: {}", e.what());
            }
        }
    }
}

// Notify error
void AcquisitionScheduler::notifyError(const std::string& error_message) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    for (const auto& callback : error_callbacks_) {
        try {
            callback(error_message);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in error callback: {}", e.what());
        }
    }
}

// Group consecutive registers
std::vector<std::vector<RegisterAddress>> AcquisitionScheduler::groupConsecutiveRegisters(
    const std::vector<RegisterAddress>& addresses) {
    
    std::vector<std::vector<RegisterAddress>> groups;
    if (addresses.empty()) return groups;
    
    auto sorted_addresses = addresses;
    std::sort(sorted_addresses.begin(), sorted_addresses.end());
    
    std::vector<RegisterAddress> current_group;
    current_group.push_back(sorted_addresses[0]);
    
    for (size_t i = 1; i < sorted_addresses.size(); ++i) {
        if (sorted_addresses[i] == sorted_addresses[i-1] + 1) {
            current_group.push_back(sorted_addresses[i]);
        } else {
            groups.push_back(current_group);
            current_group.clear();
            current_group.push_back(sorted_addresses[i]);
        }
    }
    
    if (!current_group.empty()) {
        groups.push_back(current_group);
    }
    
    return groups;
}

} // namespace ecoWatt
