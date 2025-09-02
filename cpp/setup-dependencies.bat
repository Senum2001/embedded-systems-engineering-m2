@echo off
REM Quick setup script for EcoWatt Device dependencies using vcpkg

echo ========================================
echo EcoWatt Device - Dependency Setup
echo ========================================
echo.

REM Check if vcpkg is already installed
set VCPKG_ROOT=C:\vcpkg
if "%VCPKG_ROOT%"=="" (
    set VCPKG_ROOT=C:\vcpkg
)

if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo vcpkg not found at %VCPKG_ROOT%
    echo.
    echo Would you like to install vcpkg? (y/N)
    set /p INSTALL_VCPKG=
    if /i "%INSTALL_VCPKG%"=="y" (
        echo Installing vcpkg...
        cd /d C:\
        git clone https://github.com/Microsoft/vcpkg.git
        cd vcpkg
        .\bootstrap-vcpkg.bat
        .\vcpkg integrate install
        echo vcpkg installed successfully!
    ) else (
        echo Please install vcpkg manually and run this script again.
        pause
        exit /b 1
    )
)

echo Using vcpkg at: %VCPKG_ROOT%
echo.

echo Installing required packages...
echo This may take several minutes...
echo.

REM Install all required packages
"%VCPKG_ROOT%\vcpkg.exe" install curl[core,ssl]:x64-windows sqlite3:x64-windows nlohmann-json:x64-windows spdlog:x64-windows

if errorlevel 1 (
    echo.
    echo Error: Failed to install some packages!
    echo Please check the error messages above.
    pause
    exit /b 1
)

echo.
echo ========================================
echo All dependencies installed successfully!
echo ========================================
echo.
echo You can now build the project by running:
echo   build.bat
echo.
echo Or manually with CMake:
echo   mkdir build
echo   cd build
echo   cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
echo   cmake --build . --config Release
echo.

pause
