# PowerShell build script for EcoWatt Device
# Uses vcpkg's CMake to build the project

Write-Host "========================================" -ForegroundColor Green
Write-Host "EcoWatt Device - PowerShell Build Script" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host

# Configuration
$VcpkgRoot = "C:\vcpkg"
$BuildDir = "build"
$Config = "Release"
$CMakePath = "$VcpkgRoot\downloads\tools\cmake-3.30.1-windows\cmake-3.30.1-windows-i386\bin\cmake.exe"

# Check if vcpkg CMake exists
if (-not (Test-Path $CMakePath)) {
    Write-Host "Error: CMake not found at expected vcpkg location" -ForegroundColor Red
    Write-Host "Expected: $CMakePath" -ForegroundColor Yellow
    
    # Try to find CMake in vcpkg downloads
    $CMakeSearchPath = "$VcpkgRoot\downloads\tools\cmake-*\*\bin\cmake.exe"
    $FoundCMake = Get-ChildItem $CMakeSearchPath -ErrorAction SilentlyContinue | Select-Object -First 1
    
    if ($FoundCMake) {
        $CMakePath = $FoundCMake.FullName
        Write-Host "Found CMake at: $CMakePath" -ForegroundColor Yellow
    } else {
        Write-Host "Please install CMake manually or run vcpkg again" -ForegroundColor Red
        exit 1
    }
}

Write-Host "Using CMake: $CMakePath" -ForegroundColor Cyan
Write-Host "Build Configuration: $Config" -ForegroundColor Cyan
Write-Host "Build Directory: $BuildDir" -ForegroundColor Cyan
Write-Host

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
    Write-Host "Created build directory" -ForegroundColor Green
}

# Navigate to build directory
Push-Location $BuildDir

try {
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Configuring with CMake..." -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    
    # Configure the project
    & $CMakePath .. -DCMAKE_TOOLCHAIN_FILE="$VcpkgRoot\scripts\buildsystems\vcpkg.cmake" -A x64
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Building project ($Config)..." -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    
    # Build the project
    & $CMakePath --build . --config $Config --parallel
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host
    Write-Host "Executable location: $BuildDir\$Config\EcoWattDevice.exe" -ForegroundColor Cyan
    Write-Host
    
    # Ask if user wants to run the application
    $runApp = Read-Host "Would you like to run the application now? (y/N)"
    if ($runApp -eq "y" -or $runApp -eq "Y") {
        Write-Host
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "Running EcoWatt Device..." -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
        Write-Host
        
        & ".\$Config\EcoWattDevice.exe"
    }
    
} catch {
    Write-Host "An error occurred: $_" -ForegroundColor Red
    exit 1
} finally {
    # Return to original directory
    Pop-Location
}

Write-Host
Write-Host "Build script completed!" -ForegroundColor Green
