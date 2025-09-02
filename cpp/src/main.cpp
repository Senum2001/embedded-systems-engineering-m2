/**
 * @file main.cpp
 * @brief Main application entry point for EcoWatt Device
 * @author EcoWatt Team
 * @date 2025-09-02
 */

#include "config_manager.hpp"
#include "logger.hpp"
#include "ecoWatt_device.hpp"
#include "exceptions.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <memory>

// Global device instance for signal handling
static std::unique_ptr<ecoWatt::EcoWattDevice> g_device;

/**
 * @brief Signal handler for graceful shutdown
 */
void signalHandler(int signal) {
    LOG_INFO("Received signal {}, initiating graceful shutdown...", signal);
    
    if (g_device) {
        try {
            g_device->stopAcquisition();
        } catch (const std::exception& e) {
            LOG_ERROR("Error during shutdown: {}", e.what());
        }
    }
    
    ecoWatt::Logger::shutdown();
    std::exit(signal);
}

namespace ecoWatt {

/**
 * @brief Convert Unicode units to ASCII equivalents for terminal display
 */
std::string sanitizeUnit(const std::string& unit) {
    std::string sanitized = unit;
    // Replace Unicode degree symbol with ASCII equivalent
    size_t pos = sanitized.find("°C");
    if (pos != std::string::npos) {
        sanitized.replace(pos, 3, "degC"); // "°C" is 3 bytes in UTF-8
    }
    return sanitized;
}

/**
 * @brief Print application banner
 */
void printBanner(const ConfigManager& config) {
    std::cout << "\n";
    std::cout << "+==============================================================+\n";
    std::cout << "|                        EcoWatt Device                       |\n";
    std::cout << "|              Milestone 2: Inverter SIM Integration          |\n";
    std::cout << "|                                                              |\n";
    std::cout << "|  Version: " << std::left << std::setw(50) << config.getAppVersion() << " |\n";
    std::cout << "|  Build Date: September 2, 2025                              |\n";
    std::cout << "|                                                              |\n";
    std::cout << "+==============================================================+\n";
    std::cout << "\n";
}

/**
 * @brief Print system status information
 */
void printSystemStatus(const EcoWattDevice& device) {
    try {
        auto status = device.getSystemStatus();
        
        std::cout << "\n+- System Status ----------------------------------------------+\n";
        std::cout << "| Running: " << (status.is_running ? "Yes" : "No") << std::string(48, ' ') << "|\n";
        std::cout << "| Total Polls: " << std::left << std::setw(47) << status.acquisition_stats.total_polls << "|\n";
        std::cout << "| Success Rate: " << std::left << std::setw(46) << (status.acquisition_stats.success_rate() * 100) << "%|\n";
        std::cout << "| Total Samples: " << std::left << std::setw(45) << status.storage_stats.total_samples << "|\n";
        std::cout << "| Polling Interval: " << std::left << std::setw(42) << (status.acquisition_config.polling_interval.count() / 1000) << "s|\n";
        std::cout << "+--------------------------------------------------------------+\n";
        
        // Print current readings
        auto readings = device.getCurrentReadings();
        if (!readings.empty()) {
            std::cout << "\n+- Current Readings -------------------------------------------+\n";
            for (const auto& [name, data] : readings) {
                std::string reading_line = "| " + name + ": " + 
                                         std::to_string(data.scaled_value) + " " + 
                                         sanitizeUnit(data.unit);
                std::cout << std::left << std::setw(62) << reading_line << "|\n";
            }
            std::cout << "+--------------------------------------------------------------+\n";
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get system status: {}", e.what());
    }
}

/**
 * @brief Run demonstration sequence
 */
void runDemo(EcoWattDevice& device) {
    LOG_INFO("Starting demonstration sequence...");
    
    const int demo_duration_seconds = 60;
    const int status_interval_seconds = 10;
    
    auto start_time = std::chrono::steady_clock::now();
    auto last_status_time = start_time;
    
    std::cout << "\n[*] Running demonstration for " << demo_duration_seconds << " seconds...\n";
    std::cout << "   Press Ctrl+C to stop early\n\n";
    
    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
        
        // Check if demo duration is complete
        if (elapsed.count() >= demo_duration_seconds) {
            break;
        }
        
        // Print status every interval
        auto status_elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_status_time);
        if (status_elapsed.count() >= status_interval_seconds) {
            printSystemStatus(device);
            last_status_time = current_time;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Demonstrate write operation
    std::cout << "\n[*] Demonstrating write operation...\n";
    try {
        // Set export power to 75%
        if (device.setExportPower(75)) {
            std::cout << "[+] Successfully set export power to 75%\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Reset to 50%
            if (device.setExportPower(50)) {
                std::cout << "[+] Reset export power to 50%\n";
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Write operation failed: {}", e.what());
    }
    
    // Export data
    std::cout << "\n[*] Exporting demonstration data...\n";
    try {
        device.exportData("milestone2_demo_data.csv", "csv", Duration(1));
        std::cout << "[+] Data exported to milestone2_demo_data.csv\n";
        
        device.exportData("milestone2_demo_data.json", "json", Duration(1));
        std::cout << "[+] Data exported to milestone2_demo_data.json\n";
    } catch (const std::exception& e) {
        LOG_ERROR("Data export failed: {}", e.what());
    }
    
    // Final status
    std::cout << "\n[*] Final Status:\n";
    printSystemStatus(device);
    
    LOG_INFO("Demonstration sequence completed");
}

} // namespace ecoWatt

/**
 * @brief Main application entry point
 */
int main(int argc, char* argv[]) {
    using namespace ecoWatt;
    
    try {
        // Parse command line arguments
        std::string config_file = "config.json";
        std::string env_file = ".env";
        bool demo_mode = false;
        
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_file = argv[++i];
            } else if (arg == "--env" && i + 1 < argc) {
                env_file = argv[++i];
            } else if (arg == "--demo") {
                demo_mode = true;
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: " << argv[0] << " [options]\n";
                std::cout << "Options:\n";
                std::cout << "  --config <file>  Configuration file (default: config.json)\n";
                std::cout << "  --env <file>     Environment file (default: .env)\n";
                std::cout << "  --demo           Run demonstration mode\n";
                std::cout << "  --help, -h       Show this help message\n";
                return 0;
            }
        }
        
        // Load configuration
        ConfigManager config(config_file, env_file);
        
        // Initialize logging
        Logger::initialize(config.getLoggingConfig());
        
        // Print banner
        printBanner(config);
        
        LOG_INFO("Starting {} v{}", config.getAppName(), config.getAppVersion());
        LOG_INFO("Configuration loaded from: {}", config_file);
        
        // Setup signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Create EcoWatt Device
        g_device = std::make_unique<EcoWattDevice>(config);
        
        // Test communication
        std::cout << "[*] Testing communication with Inverter SIM...\n";
        if (!g_device->testCommunication()) {
            LOG_ERROR("Communication test failed - check configuration and network connectivity");
            return 1;
        }
        std::cout << "[+] Communication test successful\n";
        
        // Start acquisition
        std::cout << "[*] Starting data acquisition...\n";
        g_device->startAcquisition();
        std::cout << "[+] Data acquisition started\n";
        
        if (demo_mode) {
            // Run demonstration
            runDemo(*g_device);
        } else {
            // Normal operation mode
            std::cout << "\n[*] EcoWatt Device is running normally\n";
            std::cout << "   Press Ctrl+C to stop\n\n";
            
            // Main loop - print status periodically
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                printSystemStatus(*g_device);
            }
        }
        
    } catch (const ConfigException& e) {
        std::cerr << "[!] Configuration Error: " << e.what() << std::endl;
        return 1;
    } catch (const EcoWattException& e) {
        std::cerr << "[!] EcoWatt Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[!] Unexpected Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
