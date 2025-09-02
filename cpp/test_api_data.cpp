/**
 * @file test_api_data.cpp
 * @brief Test program for API data reception and display
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#include "config_manager.hpp"
#include "protocol_adapter.hpp"
#include "logger.hpp"
#include "http_client.hpp"
#include "exceptions.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <map>

namespace ecoWatt {

/**
 * @brief Convert raw register value to scaled value
 */
double scaleValue(uint16_t raw_value, double gain) {
    // Per API, gain is the divisor
    return (gain != 0.0) ? static_cast<double>(raw_value) / gain
                         : static_cast<double>(raw_value);
}

/**
 * @brief Print formatted test header
 */
void printTestHeader(const std::string& test_name) {
    std::cout << "\n";
    std::cout << "+==============================================================+\n";
    std::cout << "|  " << std::left << std::setw(58) << test_name << "  |\n";
    std::cout << "+==============================================================+\n";
    std::cout << "\n";
}

/**
 * @brief Print register data in formatted table
 */
void printRegisterData(const std::map<std::string, double>& data, 
                      const std::map<std::string, std::string>& units) {
    std::cout << "+- API Data Received ------------------------------------------+\n";
    
    for (const auto& [name, value] : data) {
        auto unit_it = units.find(name);
        std::string unit = (unit_it != units.end()) ? unit_it->second : "";
        
        // Replace Unicode degree symbol
        if (unit.find("°C") != std::string::npos) {
            unit = "degC";
        }
        
        std::string line = "| " + name + ": " + 
                          std::to_string(value) + " " + unit;
        std::cout << std::left << std::setw(62) << line << "|\n";
    }
    
    std::cout << "+--------------------------------------------------------------+\n";
}

/**
 * @brief Test individual register read
 */
bool testSingleRegisterRead(ProtocolAdapter& adapter, RegisterAddress reg_addr, 
                           const std::string& reg_name, const std::string& unit, double gain) {
    try {
        std::cout << "[*] Testing register " << reg_addr << " (" << reg_name << ")...\n";
        
        auto response = adapter.readHoldingRegisters(reg_addr, 1);
        
        if (response.is_error) {
            std::cout << "[!] Error reading register " << reg_addr << ": " << response.error_message << "\n";
            return false;
        }
        
        if (response.data.size() < 2) {
            std::cout << "[!] Insufficient data received for register " << reg_addr << "\n";
            return false;
        }
        
        // Convert to 16-bit value (assuming big-endian)
        uint16_t raw_value = (response.data[0] << 8) | response.data[1];
        double scaled_value = scaleValue(raw_value, gain);
        
        std::cout << "[+] Raw value: " << raw_value << ", Scaled: " << scaled_value << " " << unit << "\n";
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "[!] Exception reading register " << reg_addr << ": " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test HTTP client connectivity
 */
bool testHttpConnectivity(const std::string& base_url) {
    try {
        std::cout << "[*] Testing HTTP connectivity to: " << base_url << "\n";
        
        HttpClient client(base_url, 5000);
        
        // Test with a simple endpoint
        nlohmann::json test_data = {
            {"slave_address", 17},
            {"function_code", 3},
            {"register_address", 0},
            {"register_count", 1}
        };
        
        auto response = client.post("/api/inverter/read", test_data.dump());
        
        if (response.isSuccess()) {
            std::cout << "[+] HTTP connectivity test successful\n";
            std::cout << "[+] Response code: " << response.status_code << "\n";
            std::cout << "[+] Response body: " << response.body << "\n";
            return true;
        } else {
            std::cout << "[!] HTTP test failed with status: " << response.status_code << "\n";
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cout << "[!] HTTP connectivity test failed: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Test complete register reading cycle
 */
void testCompleteDataCycle(ProtocolAdapter& adapter, const ConfigManager& config) {
    printTestHeader("Complete Data Acquisition Test");
    
    // Get register configurations
    auto register_configs = config.getRegisterConfigurations();
    
    std::map<std::string, double> data_values;
    std::map<std::string, std::string> data_units;
    
    std::cout << "[*] Reading all configured registers...\n\n";
    
    int successful_reads = 0;
    int total_registers = register_configs.size();
    
    for (const auto& [reg_addr, reg_config] : register_configs) {
        if (testSingleRegisterRead(adapter, reg_addr, reg_config.name, 
                                  reg_config.unit, reg_config.gain)) {
            // Store for display
            try {
                auto response = adapter.readHoldingRegisters(reg_addr, 1);
                if (!response.is_error && response.data.size() >= 2) {
                    uint16_t raw_value = (response.data[0] << 8) | response.data[1];
                    double scaled_value = scaleValue(raw_value, reg_config.gain);
                    
                    data_values[reg_config.name] = scaled_value;
                    data_units[reg_config.name] = reg_config.unit;
                    successful_reads++;
                }
            } catch (...) {
                // Continue with next register
            }
        }
        std::cout << "\n";
    }
    
    std::cout << "\n[*] Read " << successful_reads << " out of " << total_registers << " registers successfully\n\n";
    
    if (!data_values.empty()) {
        printRegisterData(data_values, data_units);
    }
}

/**
 * @brief Test data validation and scaling
 */
void testDataValidation() {
    printTestHeader("Data Validation and Scaling Test");
    
    // Test various raw values with different gains
    struct TestCase {
        uint16_t raw_value;
        double gain;
        double expected;
        std::string description;
    };
    
    std::vector<TestCase> test_cases = {
        {2308, 10.0, 23080.0, "Voltage reading (230.8V -> 23080.0V display)"},
        {40, 10.0, 400.0, "Current reading (4.0A -> 400.0A display)"},
        {5007, 100.0, 500700.0, "Frequency reading (50.07Hz -> 500700.0Hz display)"},
        {353, 10.0, 3530.0, "Temperature reading (35.3°C -> 3530.0degC display)"},
        {75, 1.0, 75.0, "Power percentage (75% -> 75.0% display)"}
    };
    
    std::cout << "+- Data Scaling Test Results ------------------------------+\n";
    
    for (const auto& test : test_cases) {
        double result = scaleValue(test.raw_value, test.gain);
        bool passed = (std::abs(result - test.expected) < 0.01);
        
        std::cout << "| " << (passed ? "[PASS]" : "[FAIL]") << " " 
                  << std::left << std::setw(52) << test.description << "|\n";
        std::cout << "|        Raw: " << std::setw(6) << test.raw_value 
                  << " Gain: " << std::setw(6) << test.gain 
                  << " Result: " << std::setw(10) << result << "    |\n";
    }
    
    std::cout << "+----------------------------------------------------------+\n";
}

} // namespace ecoWatt

/**
 * @brief Main test function
 */
int main() {
    try {
        // Initialize logging
        ecoWatt::Logger::initialize(ecoWatt::LogLevel::DEBUG, "test_api.log");
        
        std::cout << "EcoWatt Device - API Data Reception and Display Test\n";
        std::cout << "====================================================\n";
        
        // Load configuration
        ecoWatt::ConfigManager config;
        config.loadFromFile("config.json");
        
        // Test data validation first
        ecoWatt::testDataValidation();
        
        // Test HTTP connectivity
        ecoWatt::printTestHeader("HTTP Client Connectivity Test");
        std::string base_url = "http://20.15.114.131:8080";
        bool http_ok = ecoWatt::testHttpConnectivity(base_url);
        
        if (http_ok) {
            // Initialize protocol adapter
            auto protocol_adapter = std::make_shared<ecoWatt::ProtocolAdapter>(config);
            
            // Test complete data cycle
            ecoWatt::testCompleteDataCycle(*protocol_adapter, config);
            
            // Test continuous reading for a few cycles
            ecoWatt::printTestHeader("Continuous Reading Test (5 cycles)");
            
            for (int i = 1; i <= 5; ++i) {
                std::cout << "\n--- Cycle " << i << " ---\n";
                
                // Read a few key registers
                std::vector<ecoWatt::RegisterAddress> key_registers = {0, 1, 2, 7, 9}; // Voltage, Current, Frequency, Temperature, Power
                
                for (auto reg_addr : key_registers) {
                    auto reg_configs = config.getRegisterConfigurations();
                    auto it = reg_configs.find(reg_addr);
                    if (it != reg_configs.end()) {
                        ecoWatt::testSingleRegisterRead(*protocol_adapter, reg_addr, 
                                                       it->second.name, it->second.unit, it->second.gain);
                    }
                }
                
                if (i < 5) {
                    std::cout << "Waiting 2 seconds before next cycle...\n";
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
        }
        
        std::cout << "\n+==============================================================+\n";
        std::cout << "|                    Test Completed                           |\n";
        std::cout << "+==============================================================+\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
