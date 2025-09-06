# Add Google Test to the project
find_package(GTest REQUIRED)

# Include test directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../cpp/include)

# Define test source files
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_frame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_protocol_adapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_api_integration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_error_scenarios.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_data_storage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_acquisition_scheduler.cpp
)

# Define main project sources (exclude main.cpp)
set(PROJECT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/modbus_frame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/protocol_adapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/http_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/config_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/data_storage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/acquisition_scheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/ecoWatt_device.cpp
)

# Create individual test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
    # Get test name without extension and path
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    # Create test executable
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${PROJECT_SOURCES})
    
    # Link required libraries
    target_link_libraries(${TEST_NAME}
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        cpprestsdk::cpprest
        sqlite3
        spdlog::spdlog
        fmt::fmt
    )
    
    # Set C++17 standard
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)
    
    # Add compiler flags for better debugging
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${TEST_NAME} PRIVATE
            -g
            -O0
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
    
    # Add test to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    # Set test properties
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 60
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

# Create a comprehensive test suite runner
add_executable(run_all_tests
    ${TEST_SOURCES}
    ${PROJECT_SOURCES}
)

target_link_libraries(run_all_tests
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main
    cpprestsdk::cpprest
    sqlite3
    spdlog::spdlog
    fmt::fmt
)

target_compile_features(run_all_tests PRIVATE cxx_std_17)

# Add comprehensive test to CTest
add_test(NAME run_all_tests COMMAND run_all_tests)
set_tests_properties(run_all_tests PROPERTIES
    TIMEOUT 300
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Copy test configuration files
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/config.json
    COPYONLY
)

# Get list of test executables for properties
set(TEST_EXECUTABLES "")
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    list(APPEND TEST_EXECUTABLES ${TEST_NAME})
endforeach()

# Set test output directory for all test executables
set_target_properties(${TEST_EXECUTABLES} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests
)

# Enable testing
enable_testing()

# Add custom target for running tests with verbose output
add_custom_target(test_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS ${TEST_SOURCES} run_all_tests
    COMMENT "Running all tests with verbose output"
)

# Add custom target for test coverage (if supported)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH)
        # Add coverage flags
        foreach(TEST_SOURCE ${TEST_SOURCES})
            get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
            target_compile_options(${TEST_NAME} PRIVATE --coverage)
            target_link_options(${TEST_NAME} PRIVATE --coverage)
        endforeach()
        
        # Add coverage target
        add_custom_target(coverage
            COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_PATH} --list coverage.info
            COMMAND ${GENHTML_PATH} coverage.info --output-directory coverage_html
            DEPENDS ${TEST_SOURCES}
            COMMENT "Generating code coverage report"
        )
    endif()
endif()

# Print test configuration summary
message(STATUS "")
message(STATUS "Test Configuration Summary:")
message(STATUS "  Google Test Found: ${GTest_FOUND}")
message(STATUS "  Test Sources: ${TEST_SOURCES}")
message(STATUS "  Test Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Test Timeout: 60 seconds per test")
message(STATUS "  Comprehensive Test Timeout: 300 seconds")
message(STATUS "")
message(STATUS "To run tests:")
message(STATUS "  cmake --build . --target test")
message(STATUS "  OR")
message(STATUS "  ctest --output-on-failure")
message(STATUS "")
message(STATUS "To run tests with verbose output:")
message(STATUS "  make test_verbose")
message(STATUS "")
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX AND GCOV_PATH AND LCOV_PATH AND GENHTML_PATH)
    message(STATUS "Code coverage available:")
    message(STATUS "  make coverage")
    message(STATUS "")
endif()
