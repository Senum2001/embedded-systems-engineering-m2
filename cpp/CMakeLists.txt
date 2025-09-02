cmake_minimum_required(VERSION 3.16)

# ----------------------------------------
# Project
# ----------------------------------------
project(EcoWattDevice
    VERSION 2.0.0
    DESCRIPTION "EcoWatt Device C++ Implementation for Milestone 2"
    LANGUAGES CXX
)

# Make IntelliSense happy: exact compile flags for VS Code
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----------------------------------------
# (Optional) Prefer Clang on Windows via clang-cl only for Visual Studio generator
# For Ninja or other generators, let CMake choose clang++/cl based on configuration
# to avoid incompatible flags like -isystem with clang-cl.
# ----------------------------------------
if (WIN32 AND CMAKE_GENERATOR MATCHES "Visual Studio")
  find_program(LLVM_CLANG_CL NAMES clang-cl)
  if (LLVM_CLANG_CL)
    # Use clang-cl so MSVC-style flags (/W3, /Zi, etc.) still work with VS generator
    set(CMAKE_C_COMPILER  "${LLVM_CLANG_CL}" CACHE FILEPATH "" FORCE)
    set(CMAKE_CXX_COMPILER "${LLVM_CLANG_CL}" CACHE FILEPATH "" FORCE)
    message(STATUS "Using clang-cl toolchain (VS generator): ${LLVM_CLANG_CL}")
  else()
    message(STATUS "clang-cl not found; using default MSVC toolchain for VS generator")
  endif()
endif()

# ----------------------------------------
# Dependencies (vcpkg-friendly)
# ----------------------------------------
if(DEFINED CMAKE_TOOLCHAIN_FILE)
  message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# ----------------------------------------
# Sources
# ----------------------------------------
set(SOURCES
  src/config_manager.cpp
  src/protocol_adapter.cpp
  src/acquisition_scheduler.cpp
  src/data_storage.cpp
  src/ecoWatt_device.cpp
  src/modbus_frame.cpp
  src/http_client.cpp
  src/logger.cpp
  src/main.cpp
)

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

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES} ${HEADERS})

# Include paths (project headers)
target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ----------------------------------------
# Compile options per compiler
# ----------------------------------------
# Common warnings for GCC/Clang
target_compile_options(${PROJECT_NAME}
  PRIVATE
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<NOT:$<CXX_COMPILER_ID:AppleClang>>,$<NOT:$<CXX_COMPILER_ID:Clang>>>:-Wall -Wextra -Wpedantic>
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wall -Wextra -Wpedantic>
)

# MSVC / clang-cl flags
if (MSVC)
  target_compile_options(${PROJECT_NAME}
    PRIVATE
      /W3         # reasonable warnings
      /permissive-# strict C++ conformance
      /MP         # parallel build
  )
  # Debug info even for Release, if desired
  target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/Zi>")
  target_link_options(${PROJECT_NAME}    PRIVATE "$<$<CONFIG:Release>:/DEBUG>")

  # Common MSVC nuisance suppressions (optional)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    _CRT_SECURE_NO_WARNINGS
  )
endif()

# ----------------------------------------
# Link libraries
# ----------------------------------------
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    CURL::libcurl
    unofficial::sqlite3::sqlite3
    Threads::Threads
)

# Project-level defines
target_compile_definitions(${PROJECT_NAME} PRIVATE
  PROJECT_VERSION="${PROJECT_VERSION}"
  PROJECT_NAME="${PROJECT_NAME}"
)

# ----------------------------------------
# Post-build: copy runtime config files next to the executable
# ----------------------------------------
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

# (Nice-to-have) Group files in IDEs
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
