/**
 * @file modbus_frame.cpp
 * @brief Implementation of Modbus RTU frame handling
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#include "modbus_frame.hpp"
#include "logger.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace ecoWatt {

std::string ModbusFrame::createReadFrame(SlaveAddress slave_address,
                                       RegisterAddress start_address,
                                       uint16_t num_registers) {
    
    std::vector<uint8_t> data;
    
    // Add start address (big-endian)
    data.push_back((start_address >> 8) & 0xFF);
    data.push_back(start_address & 0xFF);
    
    // Add number of registers (big-endian)
    data.push_back((num_registers >> 8) & 0xFF);
    data.push_back(num_registers & 0xFF);
    
    auto frame = buildFrame(slave_address, 
                           static_cast<FunctionCode>(ModbusFunction::READ_HOLDING_REGISTERS), 
                           data);
    
    std::string frame_hex = bytesToHex(frame);
    LOG_TRACE("Created read frame: {}", frame_hex);
    
    return frame_hex;
}

std::string ModbusFrame::createWriteFrame(SlaveAddress slave_address,
                                        RegisterAddress register_address,
                                        RegisterValue value) {
    
    std::vector<uint8_t> data;
    
    // Add register address (big-endian)
    data.push_back((register_address >> 8) & 0xFF);
    data.push_back(register_address & 0xFF);
    
    // Add value (big-endian)
    data.push_back((value >> 8) & 0xFF);
    data.push_back(value & 0xFF);
    
    auto frame = buildFrame(slave_address,
                           static_cast<FunctionCode>(ModbusFunction::WRITE_SINGLE_REGISTER),
                           data);
    
    std::string frame_hex = bytesToHex(frame);
    LOG_TRACE("Created write frame: {}", frame_hex);
    
    return frame_hex;
}

ModbusResponse ModbusFrame::parseResponse(const std::string& frame_hex) {
    if (frame_hex.empty()) {
        throw ModbusException("Empty response frame");
    }
    
    LOG_TRACE("Parsing response frame: {}", frame_hex);
    
    std::vector<uint8_t> frame_bytes;
    try {
        frame_bytes = hexToBytes(frame_hex);
    } catch (const ValidationException& e) {
        throw ModbusException("Invalid hex frame: " + std::string(e.what()));
    }
    
    if (frame_bytes.size() < 5) {
        throw ModbusException("Frame too short (minimum 5 bytes required)");
    }
    
    // Validate CRC
    if (!validateFrame(frame_bytes)) {
        throw ModbusException("CRC validation failed");
    }
    
    // Extract basic fields
    SlaveAddress slave_addr = frame_bytes[0];
    FunctionCode function_code = frame_bytes[1];
    
    ModbusResponse response(slave_addr, function_code, {});
    
    // Check for error response (function code + 0x80)
    if (function_code & 0x80) {
        response.is_error = true;
        response.error_code = frame_bytes[2];
        response.function_code = function_code & 0x7F; // Remove error bit
        
        std::string error_msg = getErrorMessage(response.error_code);
        LOG_ERROR("Modbus error response: {} (0x{:02X})", error_msg, response.error_code);
        
        return response;
    }
    
    // Parse data based on function code
    if (function_code == static_cast<FunctionCode>(ModbusFunction::READ_HOLDING_REGISTERS)) {
        if (frame_bytes.size() < 4) {
            throw ModbusException("Read response too short");
        }
        
        uint8_t byte_count = frame_bytes[2];
        if (frame_bytes.size() < 3 + byte_count + 2) { // 3 header + data + 2 CRC
            throw ModbusException("Frame size mismatch with byte count");
        }
        
        response.data.assign(frame_bytes.begin() + 3, frame_bytes.begin() + 3 + byte_count);
        
    } else if (function_code == static_cast<FunctionCode>(ModbusFunction::WRITE_SINGLE_REGISTER)) {
        // Write response echoes the request (register address + value)
        response.data.assign(frame_bytes.begin() + 2, frame_bytes.end() - 2);
        
    } else {
        // Generic data extraction
        response.data.assign(frame_bytes.begin() + 2, frame_bytes.end() - 2);
    }
    
    LOG_TRACE("Successfully parsed response: slave={}, func=0x{:02X}, data_size={}", 
             slave_addr, function_code, response.data.size());
    
    return response;
}

uint16_t ModbusFrame::calculateCRC(const std::vector<uint8_t>& data) {
    uint16_t crc = 0xFFFF;
    
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

std::vector<uint8_t> ModbusFrame::hexToBytes(const std::string& hex_string) {
    if (hex_string.length() % 2 != 0) {
        throw ValidationException("Hex string length must be even");
    }
    
    std::vector<uint8_t> bytes;
    bytes.reserve(hex_string.length() / 2);
    
    for (size_t i = 0; i < hex_string.length(); i += 2) {
        char high = hex_string[i];
        char low = hex_string[i + 1];
        
        if (!isValidHexChar(high) || !isValidHexChar(low)) {
            throw ValidationException("Invalid hex character in string");
        }
        
        uint8_t byte = (hexCharToValue(high) << 4) | hexCharToValue(low);
        bytes.push_back(byte);
    }
    
    return bytes;
}

std::string ModbusFrame::bytesToHex(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    
    for (uint8_t byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    
    return ss.str();
}

bool ModbusFrame::validateFrame(const std::vector<uint8_t>& frame_bytes) {
    if (frame_bytes.size() < 5) {
        return false;
    }
    
    // Extract CRC from frame (little-endian - Modbus RTU standard)
    uint16_t received_crc = frame_bytes[frame_bytes.size() - 2] | 
                           (frame_bytes[frame_bytes.size() - 1] << 8);
    
    // Calculate CRC for frame without CRC bytes
    std::vector<uint8_t> frame_without_crc(frame_bytes.begin(), frame_bytes.end() - 2);
    uint16_t calculated_crc = calculateCRC(frame_without_crc);
    
    bool valid = (received_crc == calculated_crc);
    
    if (!valid) {
        LOG_DEBUG("CRC validation failed: received=0x{:04X}, calculated=0x{:04X}", 
                 received_crc, calculated_crc);
    }
    
    return valid;
}

std::string ModbusFrame::getErrorMessage(uint8_t error_code) {
    switch (error_code) {
        case 0x01: return "Illegal Function";
        case 0x02: return "Illegal Data Address";
        case 0x03: return "Illegal Data Value";
        case 0x04: return "Slave Device Failure";
        case 0x05: return "Acknowledge";
        case 0x06: return "Slave Device Busy";
        case 0x08: return "Memory Parity Error";
        case 0x0A: return "Gateway Path Unavailable";
        case 0x0B: return "Gateway Target Device Failed to Respond";
        default: return "Unknown Error";
    }
}

std::vector<uint8_t> ModbusFrame::buildFrame(SlaveAddress slave_address,
                                           FunctionCode function_code,
                                           const std::vector<uint8_t>& data) {
    std::vector<uint8_t> frame;
    
    // Add slave address and function code
    frame.push_back(slave_address);
    frame.push_back(function_code);
    
    // Add data
    frame.insert(frame.end(), data.begin(), data.end());
    
    // Calculate and append CRC
    appendCRC(frame);
    
    return frame;
}

void ModbusFrame::appendCRC(std::vector<uint8_t>& frame) {
    uint16_t crc = calculateCRC(frame);
    
    // Append CRC in little-endian format (Modbus RTU standard)
    frame.push_back(crc & 0xFF);        // LSB first
    frame.push_back((crc >> 8) & 0xFF); // MSB second
}

bool ModbusFrame::isValidHexChar(char c) {
    return (c >= '0' && c <= '9') || 
           (c >= 'A' && c <= 'F') || 
           (c >= 'a' && c <= 'f');
}

uint8_t ModbusFrame::hexCharToValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

char ModbusFrame::valueToHexChar(uint8_t value) {
    if (value < 10) return '0' + value;
    return 'A' + (value - 10);
}

} // namespace ecoWatt
