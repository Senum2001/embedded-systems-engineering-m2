/**
 * @file data_storage.hpp  
 * @brief Data storage system header
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

#include "types.hpp"
#include "exceptions.hpp"
#include "config_manager.hpp"
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include <deque>
#include <sqlite3.h>

namespace ecoWatt {

/**
 * @brief Memory-based data storage for fast access
 */
class MemoryDataStorage {
public:
    /**
     * @brief Constructor
     * @param max_samples_per_register Maximum samples to keep per register
     */
    explicit MemoryDataStorage(size_t max_samples_per_register = 1000);

    /**
     * @brief Store single sample
     */
    void storeSample(const AcquisitionSample& sample);

    /**
     * @brief Store multiple samples
     */
    void storeSamples(const std::vector<AcquisitionSample>& samples);

    /**
     * @brief Get samples for specific register
     * @param register_address Register address
     * @param count Maximum number of samples (0 = all)
     * @return Vector of samples (newest first)
     */
    std::vector<AcquisitionSample> getSamples(RegisterAddress register_address, 
                                            size_t count = 0) const;

    /**
     * @brief Get samples within time range
     */
    std::vector<AcquisitionSample> getSamplesByTimeRange(RegisterAddress register_address,
                                                        const TimePoint& start_time,
                                                        const TimePoint& end_time) const;

    /**
     * @brief Get latest sample for register
     */
    UniquePtr<AcquisitionSample> getLatestSample(RegisterAddress register_address) const;

    /**
     * @brief Get latest samples for all registers
     */
    std::map<RegisterAddress, AcquisitionSample> getAllLatestSamples() const;

    /**
     * @brief Clear samples for specific register or all
     */
    void clearSamples(RegisterAddress register_address = 0, bool clear_all = false);

    /**
     * @brief Get storage statistics
     */
    StorageStatistics getStatistics() const;

private:
    mutable std::mutex mutex_;
    size_t max_samples_per_register_;
    std::map<RegisterAddress, std::deque<AcquisitionSample>> samples_by_register_;
};

/**
 * @brief SQLite-based persistent data storage
 */
class SQLiteDataStorage {
public:
    /**
     * @brief Constructor
     * @param db_path Database file path
     */
    explicit SQLiteDataStorage(const std::string& db_path);

    /**
     * @brief Destructor
     */
    ~SQLiteDataStorage();

    /**
     * @brief Store single sample
     */
    void storeSample(const AcquisitionSample& sample);

    /**
     * @brief Store multiple samples (batch operation)
     */
    void storeSamples(const std::vector<AcquisitionSample>& samples);

    /**
     * @brief Get samples for specific register
     */
    std::vector<AcquisitionSample> getSamples(RegisterAddress register_address,
                                            size_t count = 0) const;

    /**
     * @brief Get samples within time range
     */
    std::vector<AcquisitionSample> getSamplesByTimeRange(RegisterAddress register_address,
                                                        const TimePoint& start_time,
                                                        const TimePoint& end_time) const;

    /**
     * @brief Store register configurations
     */
    void storeRegisterConfigs(const std::map<RegisterAddress, RegisterConfig>& configs);

    /**
     * @brief Get storage statistics
     */
    StorageStatistics getStatistics() const;

    /**
     * @brief Cleanup old data beyond retention period
     */
    void cleanupOldData(uint32_t retention_days);

    /**
     * @brief Export data to CSV
     */
    void exportToCSV(const std::string& filename,
                    const std::vector<RegisterAddress>& register_filter = {},
                    const TimePoint& start_time = TimePoint{},
                    const TimePoint& end_time = TimePoint{}) const;

private:
    void initializeDatabase();
    void executeSQL(const std::string& sql) const;
    std::string timePointToString(const TimePoint& time_point) const;
    TimePoint timePointFromString(const std::string& time_string) const;

    std::string db_path_;
    sqlite3* db_;
    mutable std::mutex mutex_;
};

/**
 * @brief Hybrid storage combining memory and persistent storage
 */
class HybridDataStorage {
public:
    /**
     * @brief Constructor
     * @param config Storage configuration
     */
    explicit HybridDataStorage(const StorageConfig& config);

    /**
     * @brief Destructor
     */
    ~HybridDataStorage();

    /**
     * @brief Store single sample in both memory and persistent storage
     */
    void storeSample(const AcquisitionSample& sample);

    /**
     * @brief Store multiple samples
     */
    void storeSamples(const std::vector<AcquisitionSample>& samples);

    /**
     * @brief Get recent samples from memory (fastest)
     */
    std::vector<AcquisitionSample> getRecentSamples(RegisterAddress register_address,
                                                   size_t count = 100) const;

    /**
     * @brief Get historical samples from persistent storage
     */
    std::vector<AcquisitionSample> getHistoricalSamples(RegisterAddress register_address,
                                                       const TimePoint& start_time,
                                                       const TimePoint& end_time) const;

    /**
     * @brief Get latest sample from memory
     */
    UniquePtr<AcquisitionSample> getLatestSample(RegisterAddress register_address) const;

    /**
     * @brief Get latest samples for all registers
     */
    std::map<RegisterAddress, AcquisitionSample> getAllLatestSamples() const;

    /**
     * @brief Store register configurations
     */
    void storeRegisterConfigs(const std::map<RegisterAddress, RegisterConfig>& configs);

    /**
     * @brief Export data to file
     */
    void exportToCSV(const std::string& filename,
                    const std::vector<RegisterAddress>& register_filter = {},
                    const TimePoint& start_time = TimePoint{},
                    const TimePoint& end_time = TimePoint{}) const;

    /**
     * @brief Export data to JSON
     */
    void exportToJSON(const std::string& filename,
                     const std::vector<RegisterAddress>& register_filter = {},
                     const TimePoint& start_time = TimePoint{},
                     const TimePoint& end_time = TimePoint{}) const;

    /**
     * @brief Get combined statistics
     */
    struct CombinedStatistics {
        StorageStatistics memory_stats;
        StorageStatistics persistent_stats;
        uint64_t total_storage_bytes;
    };
    
    CombinedStatistics getCombinedStatistics() const;

    /**
     * @brief Start background cleanup task
     */
    void startCleanupTask();

    /**
     * @brief Stop background cleanup task
     */
    void stopCleanupTask();

private:
    void cleanupLoop();

    StorageConfig config_;
    UniquePtr<MemoryDataStorage> memory_storage_;
    UniquePtr<SQLiteDataStorage> sqlite_storage_;
    
    // Background cleanup
    std::atomic<bool> cleanup_active_{false};
    UniquePtr<std::thread> cleanup_thread_;
};

} // namespace ecoWatt
