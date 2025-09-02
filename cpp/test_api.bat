@echo off
REM Test script for API data reception and display
REM This script runs focused tests on the EcoWatt Device API functionality

echo ========================================
echo EcoWatt Device - API Data Test
echo ========================================
echo.

REM Navigate to the Release build directory
cd build\Release

echo [*] Testing API connectivity and data reception...
echo.

REM Run the main application with a timeout
echo [*] Starting EcoWatt Device for API testing...
echo [*] The application will run for 30 seconds to test API data reception
echo [*] Watch for the data readings and any error messages
echo.

timeout /t 5 >nul
echo [*] Launching application...
echo.

REM Start the application and let it run for a short time
timeout 30 /nobreak | .\EcoWattDevice.exe

echo.
echo ========================================
echo API Test Results:
echo ========================================
echo.
echo [*] Check the output above for:
echo     - Configuration loading success
echo     - Communication test results
echo     - Real-time data readings
echo     - Any CURL or API errors
echo.
echo [*] If you see register readings with values, the API is working
echo [*] If you see CURL errors, there may be network connectivity issues
echo [*] Check the log file: ecoWatt_milestone2.log for detailed information
echo.

if exist ecoWatt_milestone2.log (
    echo [*] Recent log entries:
    echo ----------------------------------------
    powershell "Get-Content ecoWatt_milestone2.log | Select-Object -Last 20"
    echo ----------------------------------------
)

echo.
echo Test completed! Check the readings above to verify API data reception.
pause
