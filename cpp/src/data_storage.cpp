#include "data_storage.hpp"
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <deque>
#include <set>
#include <thread>
#include <atomic>

using namespace ecoWatt;

// MemoryDataStorage Implementation
MemoryDataStorage::MemoryDataStorage(size_t max_samples_per_register)
    : max_samples_per_register_(max_samples_per_register) {
    
    spdlog::info("MemoryDataStorage initialized with max {} samples per register", 
                 max_samples_per_register_);
}

void MemoryDataStorage::storeSample(const AcquisitionSample& sample) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& samples = samples_by_register_[sample.register_address];
    samples.push_back(sample);
    
    // Maintain size limit
    if (samples.size() > max_samples_per_register_) {
        samples.pop_front();
    }
    
    spdlog::debug("Stored sample for register {} (raw_value: {}, scaled_value: {}, timestamp: {})", 
                  sample.register_address, sample.raw_value, sample.scaled_value,
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      sample.timestamp.time_since_epoch()).count());
}

void MemoryDataStorage::storeSamples(const std::vector<AcquisitionSample>& samples) {
    for (const auto& sample : samples) {
        storeSample(sample);
    }
}

std::vector<AcquisitionSample> MemoryDataStorage::getSamples(RegisterAddress register_address, 
                                                           size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = samples_by_register_.find(register_address);
    if (it == samples_by_register_.end()) {
        return {};
    }
    
    const auto& samples = it->second;
    std::vector<AcquisitionSample> result;
    
    if (count == 0 || count >= samples.size()) {
        // Return all samples (newest first)
        result.reserve(samples.size());
        for (auto rit = samples.rbegin(); rit != samples.rend(); ++rit) {
            result.push_back(*rit);
        }
    } else {
        // Return last 'count' samples (newest first)
        result.reserve(count);
        auto start_it = samples.end() - static_cast<long>(count);
        for (auto it = samples.end() - 1; it >= start_it; --it) {
            result.push_back(*it);
        }
    }
    
    return result;
}

std::vector<AcquisitionSample> MemoryDataStorage::getSamplesByTimeRange(RegisterAddress register_address,
                                                                        const TimePoint& start_time,
                                                                        const TimePoint& end_time) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = samples_by_register_.find(register_address);
    if (it == samples_by_register_.end()) {
        return {};
    }
    
    const auto& samples = it->second;
    std::vector<AcquisitionSample> result;
    
    for (const auto& sample : samples) {
        if (sample.timestamp >= start_time && sample.timestamp <= end_time) {
            result.push_back(sample);
        }
    }
    
    // Sort by timestamp (newest first)
    std::sort(result.begin(), result.end(), 
              [](const AcquisitionSample& a, const AcquisitionSample& b) {
                  return a.timestamp > b.timestamp;
              });
    
    return result;
}

UniquePtr<AcquisitionSample> MemoryDataStorage::getLatestSample(RegisterAddress register_address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = samples_by_register_.find(register_address);
    if (it == samples_by_register_.end() || it->second.empty()) {
        return nullptr;
    }
    
    return std::make_unique<AcquisitionSample>(it->second.back());
}

std::map<RegisterAddress, AcquisitionSample> MemoryDataStorage::getAllLatestSamples() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<RegisterAddress, AcquisitionSample> result;
    
    for (const auto& pair : samples_by_register_) {
        if (!pair.second.empty()) {
            result[pair.first] = pair.second.back();
        }
    }
    
    return result;
}

void MemoryDataStorage::clearSamples(RegisterAddress register_address, bool clear_all) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (clear_all || register_address == 0) {
        samples_by_register_.clear();
    } else {
        auto it = samples_by_register_.find(register_address);
        if (it != samples_by_register_.end()) {
            it->second.clear();
        }
    }
}

StorageStatistics MemoryDataStorage::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    StorageStatistics stats;
    stats.total_samples = 0;
    
    for (const auto& pair : samples_by_register_) {
        stats.samples_by_register[pair.first] = pair.second.size();
        stats.total_samples += pair.second.size();
        
        if (!pair.second.empty()) {
            if (stats.total_samples == pair.second.size()) {
                // First register
                stats.oldest_sample_time = pair.second.front().timestamp;
                stats.newest_sample_time = pair.second.back().timestamp;
            } else {
                // Update min/max times
                if (pair.second.front().timestamp < stats.oldest_sample_time) {
                    stats.oldest_sample_time = pair.second.front().timestamp;
                }
                if (pair.second.back().timestamp > stats.newest_sample_time) {
                    stats.newest_sample_time = pair.second.back().timestamp;
                }
            }
        }
    }
    
    // Rough estimate of memory usage
    stats.storage_size_bytes = stats.total_samples * sizeof(AcquisitionSample);
    
    return stats;
}

// SQLiteDataStorage Implementation
SQLiteDataStorage::SQLiteDataStorage(const std::string& db_path)
    : db_path_(db_path), db_(nullptr) {
    
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string error = "Failed to open database: " + std::string(sqlite3_errmsg(db_));
        sqlite3_close(db_);
        throw std::runtime_error(error);
    }
    
    initializeDatabase();
    spdlog::info("SQLiteDataStorage initialized with database: {}", db_path_);
}

SQLiteDataStorage::~SQLiteDataStorage() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void SQLiteDataStorage::initializeDatabase() {
    const char* create_table_sql = R"(
        CREATE TABLE IF NOT EXISTS samples (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            register_address INTEGER NOT NULL,
            value REAL NOT NULL,
            timestamp INTEGER NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE INDEX IF NOT EXISTS idx_register_timestamp 
        ON samples(register_address, timestamp);
        
        CREATE TABLE IF NOT EXISTS register_configs (
            register_address INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            unit TEXT,
            gain REAL NOT NULL,
            description TEXT
        );
    )";
    
    executeSQL(create_table_sql);
}

void SQLiteDataStorage::executeSQL(const std::string& sql) const {
    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error_msg);
    
    if (rc != SQLITE_OK) {
        std::string error = "SQL error: " + std::string(error_msg);
        sqlite3_free(error_msg);
        throw std::runtime_error(error);
    }
}

void SQLiteDataStorage::storeSample(const AcquisitionSample& sample) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* insert_sql = R"(
        INSERT INTO samples (register_address, value, timestamp)
        VALUES (?, ?, ?)
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    
    sqlite3_bind_int(stmt, 1, static_cast<int>(sample.register_address));
    sqlite3_bind_double(stmt, 2, static_cast<double>(sample.raw_value));
    sqlite3_bind_int64(stmt, 3, std::chrono::duration_cast<std::chrono::milliseconds>(
        sample.timestamp.time_since_epoch()).count());
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert sample: " + std::string(sqlite3_errmsg(db_)));
    }
}

void SQLiteDataStorage::storeSamples(const std::vector<AcquisitionSample>& samples) {
    for (const auto& sample : samples) {
        storeSample(sample);
    }
}

std::vector<AcquisitionSample> SQLiteDataStorage::getSamples(RegisterAddress register_address,
                                                           size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string sql = "SELECT register_address, value, timestamp FROM samples WHERE register_address = ? ORDER BY timestamp DESC";
    if (count > 0) {
        sql += " LIMIT " + std::to_string(count);
    }
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    
    sqlite3_bind_int(stmt, 1, static_cast<int>(register_address));
    
    std::vector<AcquisitionSample> result;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        AcquisitionSample sample;
        sample.register_address = static_cast<RegisterAddress>(sqlite3_column_int(stmt, 0));
        sample.raw_value = static_cast<RegisterValue>(sqlite3_column_double(stmt, 1));
        sample.timestamp = TimePoint(Duration(sqlite3_column_int64(stmt, 2)));
        result.push_back(sample);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

std::vector<AcquisitionSample> SQLiteDataStorage::getSamplesByTimeRange(RegisterAddress register_address,
                                                                        const TimePoint& start_time,
                                                                        const TimePoint& end_time) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* sql = R"(
        SELECT register_address, value, timestamp 
        FROM samples 
        WHERE register_address = ? AND timestamp BETWEEN ? AND ?
        ORDER BY timestamp DESC
    )";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    
    sqlite3_bind_int(stmt, 1, static_cast<int>(register_address));
    sqlite3_bind_int64(stmt, 2, std::chrono::duration_cast<std::chrono::milliseconds>(
        start_time.time_since_epoch()).count());
    sqlite3_bind_int64(stmt, 3, std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time.time_since_epoch()).count());
    
    std::vector<AcquisitionSample> result;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        AcquisitionSample sample;
        sample.register_address = static_cast<RegisterAddress>(sqlite3_column_int(stmt, 0));
        sample.raw_value = static_cast<RegisterValue>(sqlite3_column_double(stmt, 1));
        sample.timestamp = TimePoint(Duration(sqlite3_column_int64(stmt, 2)));
        result.push_back(sample);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

void SQLiteDataStorage::storeRegisterConfigs(const std::map<RegisterAddress, RegisterConfig>& configs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* insert_sql = R"(
        INSERT OR REPLACE INTO register_configs (register_address, name, unit, gain, description)
        VALUES (?, ?, ?, ?, ?)
    )";
    
    for (const auto& config : configs) {
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        }
        
        sqlite3_bind_int(stmt, 1, static_cast<int>(config.first));
        sqlite3_bind_text(stmt, 2, config.second.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, config.second.unit.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, config.second.gain);
        sqlite3_bind_text(stmt, 5, config.second.description.c_str(), -1, SQLITE_STATIC);
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc != SQLITE_DONE) {
            throw std::runtime_error("Failed to insert config: " + std::string(sqlite3_errmsg(db_)));
        }
    }
}

StorageStatistics SQLiteDataStorage::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    StorageStatistics stats;
    
    // Get total sample count
    const char* count_sql = "SELECT COUNT(*) FROM samples";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, count_sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        stats.total_samples = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    
    // Get samples by register
    const char* register_sql = "SELECT register_address, COUNT(*) FROM samples GROUP BY register_address";
    rc = sqlite3_prepare_v2(db_, register_sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            RegisterAddress addr = static_cast<RegisterAddress>(sqlite3_column_int(stmt, 0));
            uint64_t count = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
            stats.samples_by_register[addr] = count;
        }
    }
    sqlite3_finalize(stmt);
    
    // Get time range
    const char* time_sql = "SELECT MIN(timestamp), MAX(timestamp) FROM samples";
    rc = sqlite3_prepare_v2(db_, time_sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            stats.oldest_sample_time = TimePoint(Duration(sqlite3_column_int64(stmt, 0)));
            stats.newest_sample_time = TimePoint(Duration(sqlite3_column_int64(stmt, 1)));
        }
    }
    sqlite3_finalize(stmt);
    
    // Estimate storage size (rough approximation)
    stats.storage_size_bytes = stats.total_samples * 32; // Rough estimate per row
    
    return stats;
}

void SQLiteDataStorage::cleanupOldData(uint32_t retention_days) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto cutoff_time = std::chrono::steady_clock::now() - std::chrono::hours(24 * retention_days);
    auto cutoff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        cutoff_time.time_since_epoch()).count();
    
    const char* delete_sql = "DELETE FROM samples WHERE timestamp < ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, delete_sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare cleanup statement: " + std::string(sqlite3_errmsg(db_)));
    }
    
    sqlite3_bind_int64(stmt, 1, cutoff_ms);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to cleanup old data: " + std::string(sqlite3_errmsg(db_)));
    }
}

void SQLiteDataStorage::exportToCSV(const std::string& filename,
                                   const std::vector<RegisterAddress>& register_filter,
                                   const TimePoint& start_time,
                                   const TimePoint& end_time) const {
    // Implementation would write CSV export functionality
    spdlog::info("CSV export requested to {}", filename);
}

// HybridDataStorage Implementation
HybridDataStorage::HybridDataStorage(const StorageConfig& config)
    : config_(config),
      memory_storage_(std::make_unique<MemoryDataStorage>(config.memory_retention_samples)),
      sqlite_storage_(std::make_unique<SQLiteDataStorage>(config.database_path)) {
    
    spdlog::info("HybridDataStorage initialized");
}

HybridDataStorage::~HybridDataStorage() {
    stopCleanupTask();
}

void HybridDataStorage::storeSample(const AcquisitionSample& sample) {
    // Always store in memory
    memory_storage_->storeSample(sample);
    
    // Store in SQLite based on configuration
    if (config_.enable_persistent_storage) {
        sqlite_storage_->storeSample(sample);
    }
}

void HybridDataStorage::storeSamples(const std::vector<AcquisitionSample>& samples) {
    for (const auto& sample : samples) {
        storeSample(sample);
    }
}

std::vector<AcquisitionSample> HybridDataStorage::getRecentSamples(RegisterAddress register_address,
                                                                  size_t count) const {
    return memory_storage_->getSamples(register_address, count);
}

std::vector<AcquisitionSample> HybridDataStorage::getHistoricalSamples(RegisterAddress register_address,
                                                                       const TimePoint& start_time,
                                                                       const TimePoint& end_time) const {
    return sqlite_storage_->getSamplesByTimeRange(register_address, start_time, end_time);
}

UniquePtr<AcquisitionSample> HybridDataStorage::getLatestSample(RegisterAddress register_address) const {
    // Try memory first
    auto latest = memory_storage_->getLatestSample(register_address);
    if (latest) {
        return latest;
    }
    
    // Fall back to SQLite
    auto sqlite_samples = sqlite_storage_->getSamples(register_address, 1);
    if (!sqlite_samples.empty()) {
        return std::make_unique<AcquisitionSample>(sqlite_samples.front());
    }
    
    return nullptr;
}

std::map<RegisterAddress, AcquisitionSample> HybridDataStorage::getAllLatestSamples() const {
    return memory_storage_->getAllLatestSamples();
}

void HybridDataStorage::storeRegisterConfigs(const std::map<RegisterAddress, RegisterConfig>& configs) {
    sqlite_storage_->storeRegisterConfigs(configs);
}

void HybridDataStorage::exportToCSV(const std::string& filename,
                                   const std::vector<RegisterAddress>& register_filter,
                                   const TimePoint& start_time,
                                   const TimePoint& end_time) const {
    sqlite_storage_->exportToCSV(filename, register_filter, start_time, end_time);
}

void HybridDataStorage::exportToJSON(const std::string& filename,
                                    const std::vector<RegisterAddress>& register_filter,
                                    const TimePoint& start_time,
                                    const TimePoint& end_time) const {
    // Implementation would write JSON export functionality
    spdlog::info("JSON export requested to {}", filename);
}

HybridDataStorage::CombinedStatistics HybridDataStorage::getCombinedStatistics() const {
    CombinedStatistics combined;
    combined.memory_stats = memory_storage_->getStatistics();
    combined.persistent_stats = sqlite_storage_->getStatistics();
    combined.total_storage_bytes = combined.memory_stats.storage_size_bytes + 
                                  combined.persistent_stats.storage_size_bytes;
    
    return combined;
}

void HybridDataStorage::startCleanupTask() {
    if (!cleanup_active_.load()) {
        cleanup_active_.store(true);
        cleanup_thread_ = std::make_unique<std::thread>(&HybridDataStorage::cleanupLoop, this);
    }
}

void HybridDataStorage::stopCleanupTask() {
    if (cleanup_active_.load()) {
        cleanup_active_.store(false);
        if (cleanup_thread_ && cleanup_thread_->joinable()) {
            cleanup_thread_->join();
        }
        cleanup_thread_.reset();
    }
}

void HybridDataStorage::cleanupLoop() {
    while (cleanup_active_.load()) {
        try {
            // Cleanup old SQLite data if configured
            if (config_.data_retention_days > 0) {
                sqlite_storage_->cleanupOldData(config_.data_retention_days);
            }
            
            // Sleep for cleanup interval
            std::this_thread::sleep_for(std::chrono::hours(24)); // Daily cleanup
        } catch (const std::exception& e) {
            spdlog::error("Cleanup task error: {}", e.what());
            std::this_thread::sleep_for(std::chrono::minutes(30)); // Retry after 30 minutes on error
        }
    }
}
