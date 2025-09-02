/**
 * @file modbus_frame.hpp
 * @brief Modbus RTU frame handling
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#pragma once

#include "types.hpp"
#include "exceptions.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace ecoWatt {

/**
 * @brief Modbus RTU frame builder and parser
 */
class ModbusFrame {
public:
    /**
     * @brief Create read holding registers frame
     * @param slave_address Slave device address
     * @param start_address Starting register address
     * @param num_registers Number of registers to read
     * @return Hex string representation of frame
     */
    static std::string createReadFrame(SlaveAddress slave_address,
                                     RegisterAddress start_address,
                                     uint16_t num_registers);

    /**
     * @brief Create write single register frame
     * @param slave_address Slave device address
     * @param register_address Register address to write
     * @param value Value to write
     * @return Hex string representation of frame
     */
    static std::string createWriteFrame(SlaveAddress slave_address,
                                      RegisterAddress register_address,
                                      RegisterValue value);

    /**
     * @brief Parse response frame
     * @param frame_hex Hex string representation of response frame
     * @return Parsed Modbus response
     * @throws ModbusException if frame is invalid
     */
    static ModbusResponse parseResponse(const std::string& frame_hex);

    /**
     * @brief Calculate Modbus RTU CRC
     * @param data Data bytes to calculate CRC for
     * @return CRC value
     */
    static uint16_t calculateCRC(const std::vector<uint8_t>& data);

    /**
     * @brief Convert hex string to bytes
     * @param hex_string Hex string to convert
     * @return Vector of bytes
     * @throws ValidationException if hex string is invalid
     */
    static std::vector<uint8_t> hexToBytes(const std::string& hex_string);

    /**
     * @brief Convert bytes to hex string
     * @param data Bytes to convert
     * @return Hex string representation
     */
    static std::string bytesToHex(const std::vector<uint8_t>& data);

    /**
     * @brief Validate frame structure and CRC
     * @param frame_bytes Frame bytes to validate
     * @return True if frame is valid
     */
    static bool validateFrame(const std::vector<uint8_t>& frame_bytes);

    /**
     * @brief Get error message for Modbus exception code
     * @param error_code Modbus exception code
     * @return Error description
     */
    static std::string getErrorMessage(uint8_t error_code);

private:
    // Helper functions
    static std::vector<uint8_t> buildFrame(SlaveAddress slave_address,
                                         FunctionCode function_code,
                                         const std::vector<uint8_t>& data);
    
    static void appendCRC(std::vector<uint8_t>& frame);
    
    static bool isValidHexChar(char c);
    static uint8_t hexCharToValue(char c);
    static char valueToHexChar(uint8_t value);
};

} // namespace ecoWatt
