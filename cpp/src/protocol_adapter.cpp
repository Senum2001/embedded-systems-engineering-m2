/**
 * @file protocol_adapter.cpp
 * @brief Implementation of protocol adapter
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#include "protocol_adapter.hpp"
#include "logger.hpp"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

namespace ecoWatt {

ProtocolAdapter::ProtocolAdapter(const ConfigManager& config)
    : modbus_config_(config.getModbusConfig()),
      api_config_(config.getApiConfig()) {
    
    // Initialize HTTP client
    http_client_ = std::make_unique<HttpClient>(api_config_.base_url, 
                                               modbus_config_.timeout.count());
    
    // Set default headers
    std::map<std::string, std::string> headers = {
        {"Authorization", api_config_.api_key},
        {"Content-Type", api_config_.content_type},
        {"Accept", api_config_.accept}
    };
    http_client_->setDefaultHeaders(headers);
    
    LOG_INFO("Protocol adapter initialized with slave address {}", modbus_config_.slave_address);
}

std::vector<RegisterValue> ProtocolAdapter::readRegisters(RegisterAddress start_address,
                                                         uint16_t num_registers) {
    if (num_registers == 0 || num_registers > 125) {
        throw ModbusException("Invalid number of registers: " + std::to_string(num_registers));
    }
    
    LOG_DEBUG("Reading {} registers starting from address {}", num_registers, start_address);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    request_start_time_ = start_time;
    
    try {
        // Create Modbus frame
        std::string request_frame = ModbusFrame::createReadFrame(
            modbus_config_.slave_address, start_address, num_registers);
        
        // Send request
        std::string response_frame = sendRequest(api_config_.read_endpoint, request_frame);
        
        // Parse response
        ModbusResponse response = ModbusFrame::parseResponse(response_frame);
        
        if (response.is_error) {
            std::string error_msg = ModbusFrame::getErrorMessage(response.error_code);
            throw ModbusException(response.error_code, error_msg);
        }
        
        // Extract register values
        std::vector<RegisterValue> values = parseRegisterValues(response.data);
        
        if (values.size() != num_registers) {
            throw ModbusException("Register count mismatch: expected " + 
                                std::to_string(num_registers) + ", got " + 
                                std::to_string(values.size()));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<Duration>(end_time - start_time);
        
        updateStats(true, duration);
        
        LOG_DEBUG("Successfully read {} registers in {}ms", values.size(), duration.count());
        return values;
        
    } catch (const ModbusException&) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<Duration>(end_time - start_time);
        updateStats(false, duration);
        throw;
    } catch (const std::exception& e) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<Duration>(end_time - start_time);
        updateStats(false, duration);
        throw ModbusException("Read operation failed: " + std::string(e.what()));
    }
}

bool ProtocolAdapter::writeRegister(RegisterAddress register_address, RegisterValue value) {
    LOG_DEBUG("Writing value {} to register {}", value, register_address);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    request_start_time_ = start_time;
    
    try {
        // Create Modbus frame
        std::string request_frame = ModbusFrame::createWriteFrame(
            modbus_config_.slave_address, register_address, value);
        
        // Send request
        std::string response_frame = sendRequest(api_config_.write_endpoint, request_frame);
        
        // Parse response
        ModbusResponse response = ModbusFrame::parseResponse(response_frame);
        
        if (response.is_error) {
            std::string error_msg = ModbusFrame::getErrorMessage(response.error_code);
            throw ModbusException(response.error_code, error_msg);
        }
        
        // Verify write response (should echo the request)
        if (response.function_code == static_cast<FunctionCode>(ModbusFunction::WRITE_SINGLE_REGISTER)) {
            if (response.data.size() >= 4) {
                uint16_t written_addr = (response.data[0] << 8) | response.data[1];
                uint16_t written_value = (response.data[2] << 8) | response.data[3];
                
                if (written_addr != register_address || written_value != value) {
                    throw ModbusException("Write verification failed: expected addr=" + 
                                        std::to_string(register_address) + ", value=" + 
                                        std::to_string(value) + ", got addr=" + 
                                        std::to_string(written_addr) + ", value=" + 
                                        std::to_string(written_value));
                }
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<Duration>(end_time - start_time);
        
        updateStats(true, duration);
        
        LOG_DEBUG("Successfully wrote value {} to register {} in {}ms", 
                 value, register_address, duration.count());
        return true;
        
    } catch (const ModbusException&) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<Duration>(end_time - start_time);
        updateStats(false, duration);
        throw;
    } catch (const std::exception& e) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<Duration>(end_time - start_time);
        updateStats(false, duration);
        throw ModbusException("Write operation failed: " + std::string(e.what()));
    }
}

bool ProtocolAdapter::testCommunication() {
    LOG_INFO("Testing communication with inverter SIM...");
    
    try {
        // Test read operation (read 2 registers starting from address 0)
        std::vector<RegisterValue> values = readRegisters(0, 2);
        
        if (values.size() == 2) {
            LOG_INFO("Communication test successful - read {} registers", values.size());
            
            // Test write operation (write to export power register)
            RegisterValue original_value = readRegisters(8, 1)[0];
            LOG_DEBUG("Original export power value: {}", original_value);
            
            // Write a test value
            RegisterValue test_value = 50;
            if (writeRegister(8, test_value)) {
                LOG_DEBUG("Write test successful");
                
                // Restore original value
                writeRegister(8, original_value);
                LOG_DEBUG("Restored original value");
                
                LOG_INFO("Communication test completed successfully");
                return true;
            }
        }
        
        LOG_ERROR("Communication test failed - unexpected response");
        return false;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Communication test failed: {}", e.what());
        return false;
    }
}

void ProtocolAdapter::resetStatistics() {
    stats_ = CommunicationStats{};
    LOG_DEBUG("Communication statistics reset");
}

std::string ProtocolAdapter::sendRequest(const std::string& endpoint, const std::string& frame) {
    // Create JSON payload
    nlohmann::json payload;
    payload["frame"] = frame;
    std::string json_data = payload.dump();
    
    uint32_t attempt = 0;
    std::string last_error;
    
    while (attempt < modbus_config_.max_retries) {
        try {
            LOG_TRACE("Sending request (attempt {}): {}", attempt + 1, frame);
            
            HttpResponse response = http_client_->post(endpoint, json_data);
            
            if (response.isSuccess()) {
                // Parse JSON response
                try {
                    nlohmann::json response_json = nlohmann::json::parse(response.body);
                    std::string response_frame = response_json.value("frame", "");
                    
                    if (!response_frame.empty()) {
                        LOG_TRACE("Received response: {}", response_frame);
                        return response_frame;
                    } else {
                        throw HttpException("Empty frame in response");
                    }
                    
                } catch (const nlohmann::json::exception& e) {
                    throw HttpException("Invalid JSON response: " + std::string(e.what()));
                }
            } else {
                throw HttpException(response.status_code, "HTTP request failed: " + response.body);
            }
            
        } catch (const HttpException& e) {
            last_error = e.what();
            LOG_WARN("Request attempt {} failed: {}", attempt + 1, last_error);
            
            if (attempt > 0) {
                updateStats(false, Duration(0), true); // Mark as retry
            }
            
            attempt++;
            
            if (attempt < modbus_config_.max_retries) {
                LOG_DEBUG("Retrying in {}ms...", modbus_config_.retry_delay.count());
                std::this_thread::sleep_for(modbus_config_.retry_delay);
            }
        }
    }
    
    throw ModbusException("Request failed after " + std::to_string(modbus_config_.max_retries) + 
                         " attempts. Last error: " + last_error);
}

std::vector<RegisterValue> ProtocolAdapter::parseRegisterValues(const std::vector<uint8_t>& data) {
    if (data.size() % 2 != 0) {
        throw ModbusException("Invalid data length for register values");
    }
    
    std::vector<RegisterValue> values;
    values.reserve(data.size() / 2);
    
    for (size_t i = 0; i < data.size(); i += 2) {
        // Registers are stored in big-endian format
        RegisterValue value = (data[i] << 8) | data[i + 1];
        values.push_back(value);
    }
    
    return values;
}

void ProtocolAdapter::updateStats(bool success, Duration response_time, bool was_retry) {
    stats_.total_requests++;
    
    if (success) {
        stats_.successful_requests++;
    } else {
        stats_.failed_requests++;
    }
    
    if (was_retry) {
        stats_.retry_attempts++;
    }
    
    // Update average response time (simple moving average)
    if (response_time.count() > 0) {
        if (stats_.average_response_time.count() == 0) {
            stats_.average_response_time = response_time;
        } else {
            auto avg_ms = stats_.average_response_time.count();
            auto new_ms = response_time.count();
            stats_.average_response_time = Duration((avg_ms + new_ms) / 2);
        }
    }
}

} // namespace ecoWatt
