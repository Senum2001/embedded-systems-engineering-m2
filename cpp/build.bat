@echo off
REM EcoWatt Device C++ Build Script for Windows
REM EN4440 Embedded Systems Engineering - Milestone 2

echo ========================================
echo EcoWatt Device C++ Build Script
echo ========================================
echo.

REM Check if we're in the correct directory
if not exist "CMakeLists.txt" (
    echo Error: CMakeLists.txt not found!
    echo Please run this script from the cpp directory.
    pause
    exit /b 1
)

REM Set default configuration
set BUILD_CONFIG=Release
set BUILD_DIR=build
set VCPKG_ROOT=C:\vcpkg

REM Parse command line arguments
:parse_args
if "%1"=="" goto end_parse
if "%1"=="--debug" (
    set BUILD_CONFIG=Debug
    shift
    goto parse_args
)
if "%1"=="--vcpkg" (
    set VCPKG_ROOT=%2
    shift
    shift
    goto parse_args
)
if "%1"=="--clean" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    shift
    goto parse_args
)
if "%1"=="--help" (
    echo Usage: build.bat [options]
    echo Options:
    echo   --debug      Build in Debug configuration ^(default: Release^)
    echo   --vcpkg PATH Specify vcpkg root directory ^(default: C:\vcpkg^)
    echo   --clean      Clean build directory before building
    echo   --help       Show this help message
    echo.
    echo Examples:
    echo   build.bat                    # Build Release version
    echo   build.bat --debug            # Build Debug version
    echo   build.bat --clean --debug    # Clean and build Debug
    echo   build.bat --vcpkg D:\vcpkg   # Use custom vcpkg path
    pause
    exit /b 0
)
echo Warning: Unknown argument %1
shift
goto parse_args
:end_parse

echo Configuration: %BUILD_CONFIG%
echo Build Directory: %BUILD_DIR%
echo vcpkg Root: %VCPKG_ROOT%
echo.

REM Check for required tools
echo Checking prerequisites...

REM Check for CMake
cmake --version >nul 2>&1
if errorlevel 1 (
    echo Error: CMake not found! Please install CMake and add it to PATH.
    echo Download from: https://cmake.org/download/
    pause
    exit /b 1
)
echo   ✓ CMake found

REM Check for Visual Studio
where cl.exe >nul 2>&1
if errorlevel 1 (
    echo Error: Visual Studio C++ compiler not found!
    echo Please install Visual Studio with C++ development tools.
    echo Or run this script from a Visual Studio Developer Command Prompt.
    pause
    exit /b 1
)
echo   ✓ Visual Studio C++ compiler found

REM Check for vcpkg
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo Error: vcpkg not found at %VCPKG_ROOT%
    echo Please install vcpkg or specify correct path with --vcpkg option.
    echo.
    echo To install vcpkg:
    echo   git clone https://github.com/Microsoft/vcpkg.git
    echo   cd vcpkg
    echo   .\bootstrap-vcpkg.bat
    echo   .\vcpkg integrate install
    pause
    exit /b 1
)
echo   ✓ vcpkg found

REM Check if required packages are installed
echo Checking vcpkg packages...
"%VCPKG_ROOT%\vcpkg.exe" list curl >nul 2>&1
if errorlevel 1 (
    echo Warning: libcurl not found in vcpkg
    echo Installing required packages...
    "%VCPKG_ROOT%\vcpkg.exe" install curl[core,ssl]:x64-windows
    if errorlevel 1 (
        echo Error: Failed to install libcurl
        pause
        exit /b 1
    )
)
echo   ✓ libcurl found

"%VCPKG_ROOT%\vcpkg.exe" list sqlite3 >nul 2>&1
if errorlevel 1 (
    echo Warning: SQLite3 not found in vcpkg
    echo Installing SQLite3...
    "%VCPKG_ROOT%\vcpkg.exe" install sqlite3:x64-windows
    if errorlevel 1 (
        echo Error: Failed to install SQLite3
        pause
        exit /b 1
    )
)
echo   ✓ SQLite3 found

"%VCPKG_ROOT%\vcpkg.exe" list nlohmann-json >nul 2>&1
if errorlevel 1 (
    echo Warning: nlohmann-json not found in vcpkg
    echo Installing nlohmann-json...
    "%VCPKG_ROOT%\vcpkg.exe" install nlohmann-json:x64-windows
    if errorlevel 1 (
        echo Error: Failed to install nlohmann-json
        pause
        exit /b 1
    )
)
echo   ✓ nlohmann-json found

"%VCPKG_ROOT%\vcpkg.exe" list spdlog >nul 2>&1
if errorlevel 1 (
    echo Warning: spdlog not found in vcpkg
    echo Installing spdlog...
    "%VCPKG_ROOT%\vcpkg.exe" install spdlog:x64-windows
    if errorlevel 1 (
        echo Error: Failed to install spdlog
        pause
        exit /b 1
    )
)
echo   ✓ spdlog found

echo.
echo Prerequisites check completed successfully!
echo.

REM Check for configuration files
echo Checking configuration files...
if not exist ".env" (
    echo Warning: .env file not found!
    echo Creating template .env file...
    echo # EcoWatt Device Configuration > .env
    echo INVERTER_API_KEY=your_api_key_here >> .env
    echo INVERTER_API_BASE_URL=http://20.15.114.131:8080 >> .env
    echo DATABASE_PATH=ecoWatt_milestone2.db >> .env
    echo LOG_LEVEL=INFO >> .env
    echo LOG_FILE=ecoWatt_milestone2.log >> .env
    echo DEFAULT_SLAVE_ADDRESS=17 >> .env
    echo MAX_RETRIES=3 >> .env
    echo REQUEST_TIMEOUT_MS=5000 >> .env
    echo RETRY_DELAY_MS=1000 >> .env
    echo.
    echo Please edit .env file with your API key and configuration!
)
echo   ✓ .env file ready

if not exist "config.json" (
    echo Warning: config.json not found!
    echo Please create config.json file with application configuration.
    echo See README.md for configuration examples.
)
echo   ✓ Configuration files checked

echo.

REM Create build directory
echo Creating build directory...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Configure with CMake
echo.
echo ========================================
echo Configuring with CMake...
echo ========================================
cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -A x64
if errorlevel 1 (
    echo.
    echo Error: CMake configuration failed!
    echo Please check the error messages above.
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Building project (%BUILD_CONFIG%)...
echo ========================================
cmake --build . --config %BUILD_CONFIG% --parallel
if errorlevel 1 (
    echo.
    echo Error: Build failed!
    echo Please check the error messages above.
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Executable location: %BUILD_DIR%\%BUILD_CONFIG%\EcoWattDevice.exe
echo.

REM Go back to original directory
cd ..

REM Check if we should run the application
echo Would you like to run the application now? (y/N)
set /p RUN_APP=
if /i "%RUN_APP%"=="y" (
    echo.
    echo ========================================
    echo Running EcoWatt Device...
    echo ========================================
    echo.
    "%BUILD_DIR%\%BUILD_CONFIG%\EcoWattDevice.exe"
)

echo.
echo Build script completed!
echo.
echo To run the application manually:
echo   cd %BUILD_DIR%\%BUILD_CONFIG%
echo   EcoWattDevice.exe
echo.
echo To rebuild:
echo   build.bat --clean
echo.
echo For debug build:
echo   build.bat --debug
echo.

pause
