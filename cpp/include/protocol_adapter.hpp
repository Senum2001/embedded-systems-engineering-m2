/**
 * @file protocol_adapter.hpp
 * @brief Protocol adapter for Modbus RTU communication
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

#include "types.hpp"
#include "exceptions.hpp"
#include "http_client.hpp"
#include "modbus_frame.hpp"
#include "config_manager.hpp"
#include <vector>
#include <memory>

namespace ecoWatt {

/**
 * @brief Protocol adapter for Modbus RTU communication over HTTP API
 */
class ProtocolAdapter {
public:
    /**
     * @brief Constructor
     * @param config Configuration manager instance
     */
    explicit ProtocolAdapter(const ConfigManager& config);

    /**
     * @brief Destructor
     */
    ~ProtocolAdapter() = default;

    /**
     * @brief Read holding registers from inverter
     * @param start_address Starting register address
     * @param num_registers Number of registers to read
     * @return Vector of register values
     * @throws ModbusException on communication or protocol error
     */
    std::vector<RegisterValue> readRegisters(RegisterAddress start_address, 
                                           uint16_t num_registers);

    /**
     * @brief Write single register to inverter
     * @param register_address Register address to write
     * @param value Value to write
     * @return True if successful
     * @throws ModbusException on communication or protocol error
     */
    bool writeRegister(RegisterAddress register_address, RegisterValue value);

    /**
     * @brief Test communication with inverter
     * @return True if communication is working
     */
    bool testCommunication();

    /**
     * @brief Get communication statistics
     */
    struct CommunicationStats {
        uint64_t total_requests = 0;
        uint64_t successful_requests = 0;
        uint64_t failed_requests = 0;
        uint64_t retry_attempts = 0;
        Duration average_response_time = Duration(0);
        
        double success_rate() const {
            return total_requests > 0 ? 
                static_cast<double>(successful_requests) / total_requests : 0.0;
        }
    };

    /**
     * @brief Get communication statistics
     */
    const CommunicationStats& getStatistics() const { return stats_; }

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

private:
    /**
     * @brief Send HTTP request with retry logic
     * @param endpoint API endpoint
     * @param frame Modbus frame as hex string
     * @return Response frame as hex string
     * @throws ModbusException on failure after all retries
     */
    std::string sendRequest(const std::string& endpoint, const std::string& frame);

    /**
     * @brief Parse register values from response data
     * @param data Response data bytes
     * @return Vector of register values
     */
    std::vector<RegisterValue> parseRegisterValues(const std::vector<uint8_t>& data);

    /**
     * @brief Update statistics
     */
    void updateStats(bool success, Duration response_time, bool was_retry = false);

    // Configuration
    ModbusConfig modbus_config_;
    ApiConfig api_config_;
    
    // HTTP client
    UniquePtr<HttpClient> http_client_;
    
    // Statistics
    CommunicationStats stats_;
    
    // Timing for statistics
    std::chrono::high_resolution_clock::time_point request_start_time_;
};

} // namespace ecoWatt
