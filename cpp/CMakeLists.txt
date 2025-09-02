cmake_minimum_required(VERSION 3.16)

# Use Clang on Windows if available
if (WIN32)
    set(CMAKE_C_COMPILER "clang")
    set(CMAKE_CXX_COMPILER "clang++")
endif()

# Project configuration
project(EcoWattDevice 
    VERSION 2.0.0
    DESCRIPTION "EcoWatt Device C++ Implementation for Milestone 2"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Add compile options
if(MSVC)
    add_compile_options(/W3)  # Use W3 instead of W4 to reduce excessive warnings
    # Enable parallel compilation
    add_compile_options(/MP)
    # Add debug info for Release builds
    add_compile_options(/Zi)
    add_link_options(/DEBUG)
    # Disable specific warnings for external libraries
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # Disable specific warnings that are common with vcpkg libraries
    add_compile_options(/wd4244 /wd4100 /wd4456)  # Disable conversion and unused parameter warnings
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Use vcpkg for dependency management
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Find required packages using vcpkg
find_package(Threads REQUIRED)

# Find cURL - vcpkg provides CURL::libcurl
find_package(CURL REQUIRED)
if(CURL_FOUND)
    message(STATUS "Found cURL: ${CURL_VERSION}")
    message(STATUS "cURL include dirs: ${CURL_INCLUDE_DIRS}")
    message(STATUS "cURL libraries: ${CURL_LIBRARIES}")
endif()

# Find SQLite3 - vcpkg provides unofficial::sqlite3::sqlite3
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# Find nlohmann/json - vcpkg provides nlohmann_json::nlohmann_json
find_package(nlohmann_json CONFIG REQUIRED)

# Find spdlog - vcpkg provides spdlog::spdlog
find_package(spdlog CONFIG REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/config_manager.cpp
    src/protocol_adapter.cpp
    src/acquisition_scheduler.cpp
    src/data_storage.cpp
    src/ecoWatt_device.cpp
    src/modbus_frame.cpp
    src/http_client.cpp
    src/logger.cpp
)

# Header files
set(HEADERS
    include/config_manager.hpp
    include/protocol_adapter.hpp
    include/acquisition_scheduler.hpp
    include/data_storage.hpp
    include/ecoWatt_device.hpp
    include/modbus_frame.hpp
    include/http_client.hpp
    include/logger.hpp
    include/types.hpp
    include/exceptions.hpp
)

# Create main executable
add_executable(${PROJECT_NAME} src/main.cpp ${SOURCES} ${HEADERS})


# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)


# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    CURL::libcurl
    unofficial::sqlite3::sqlite3
    Threads::Threads
)


# Compiler definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    PROJECT_VERSION="${PROJECT_VERSION}"
    PROJECT_NAME="${PROJECT_NAME}"
)

# Copy configuration files to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/config.json
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.json
    COMMENT "Copying config.json to build directory"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/.env
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/.env
    COMMENT "Copying .env to build directory"
)
