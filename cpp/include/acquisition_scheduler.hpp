/**
 * @file acquisition_scheduler.hpp
 * @brief Data acquisition scheduler header
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

#include "types.hpp"
#include "exceptions.hpp"
#include "protocol_adapter.hpp"
#include "config_manager.hpp"
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <deque>

namespace ecoWatt {

/**
 * @brief Data acquisition scheduler with configurable polling
 */
class AcquisitionScheduler {
public:
    // Callback types
    using SampleCallback = std::function<void(const AcquisitionSample&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    /**
     * @brief Constructor
     * @param protocol_adapter Protocol adapter for communication
     * @param config Configuration manager
     */
    AcquisitionScheduler(SharedPtr<ProtocolAdapter> protocol_adapter,
                        const ConfigManager& config);

    /**
     * @brief Destructor
     */
    ~AcquisitionScheduler();

    /**
     * @brief Start automatic polling
     */
    void startPolling();

    /**
     * @brief Stop automatic polling
     */
    void stopPolling();

    /**
     * @brief Check if polling is active
     */
    bool isPolling() const { return polling_active_.load(); }

    /**
     * @brief Set polling interval
     * @param interval Polling interval
     */
    void setPollingInterval(Duration interval);

    /**
     * @brief Set minimum required registers
     * @param registers List of register addresses
     */
    void setMinimumRegisters(const std::vector<RegisterAddress>& registers);

    /**
     * @brief Configure registers to monitor
     * @param register_configs Register configuration map
     */
    void configureRegisters(const std::map<RegisterAddress, RegisterConfig>& register_configs);

    /**
     * @brief Add sample callback
     * @param callback Function to call when new sample is acquired
     */
    void addSampleCallback(SampleCallback callback);

    /**
     * @brief Add error callback
     * @param callback Function to call when error occurs
     */
    void addErrorCallback(ErrorCallback callback);

    /**
     * @brief Read single register manually
     * @param address Register address
     * @return Acquisition sample or nullptr if failed
     */
    UniquePtr<AcquisitionSample> readSingleRegister(RegisterAddress address);

    /**
     * @brief Read multiple registers manually
     * @param addresses List of register addresses
     * @return Vector of successful samples
     */
    std::vector<AcquisitionSample> readMultipleRegisters(const std::vector<RegisterAddress>& addresses);

    /**
     * @brief Perform write operation
     * @param register_address Register address to write
     * @param value Value to write
     * @return True if successful
     */
    bool performWriteOperation(RegisterAddress register_address, RegisterValue value);

    /**
     * @brief Get recent samples from internal buffer
     * @param count Maximum number of samples to return
     * @return Vector of recent samples
     */
    std::vector<AcquisitionSample> getRecentSamples(size_t count = 100);

    /**
     * @brief Get samples for specific register
     * @param register_address Register address to filter by
     * @param count Maximum number of samples to return
     * @return Vector of samples for specified register
     */
    std::vector<AcquisitionSample> getSamplesByRegister(RegisterAddress register_address, 
                                                       size_t count = 100);

    /**
     * @brief Get acquisition statistics
     */
    const AcquisitionStatistics& getStatistics() const { return statistics_; }

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

    /**
     * @brief Get current configuration
     */
    const AcquisitionConfig& getConfig() const { return config_; }

private:
    /**
     * @brief Main polling loop (runs in separate thread)
     */
    void pollingLoop();

    /**
     * @brief Perform single poll cycle
     */
    void performPollCycle();

    /**
     * @brief Store sample in internal buffer and notify callbacks
     */
    void storeSample(const AcquisitionSample& sample);

    /**
     * @brief Notify error callbacks
     */
    void notifyError(const std::string& error_message);

    /**
     * @brief Group consecutive register addresses for efficient reading
     */
    std::vector<std::vector<RegisterAddress>> groupConsecutiveRegisters(
        const std::vector<RegisterAddress>& addresses);

    // Dependencies
    SharedPtr<ProtocolAdapter> protocol_adapter_;
    std::map<RegisterAddress, RegisterConfig> register_configs_;

    // Configuration
    AcquisitionConfig config_;
    std::vector<RegisterAddress> minimum_registers_;

    // Threading
    std::atomic<bool> polling_active_{false};
    std::atomic<bool> stop_requested_{false};
    UniquePtr<std::thread> polling_thread_;

    // Sample storage (internal buffer)
    std::deque<AcquisitionSample> sample_buffer_;
    mutable std::mutex buffer_mutex_;
    size_t max_buffer_size_;

    // Callbacks
    std::vector<SampleCallback> sample_callbacks_;
    std::vector<ErrorCallback> error_callbacks_;
    mutable std::mutex callbacks_mutex_;

    // Statistics
    AcquisitionStatistics statistics_;
    mutable std::mutex stats_mutex_;
};

} // namespace ecoWatt
