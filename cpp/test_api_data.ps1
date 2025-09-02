# PowerShell script for comprehensive API data testing
# Tests API connectivity, data reception, and display functionality

Write-Host "========================================" -ForegroundColor Green
Write-Host "EcoWatt Device - API Data Reception Test" -ForegroundColor Green  
Write-Host "========================================" -ForegroundColor Green
Write-Host

# Navigate to build directory
if (-not (Test-Path "build\Release")) {
    Write-Host "Error: Build directory not found. Please build the project first." -ForegroundColor Red
    Write-Host "Run: .\build.ps1" -ForegroundColor Yellow
    exit 1
}

Push-Location "build\Release"

try {
    Write-Host "[*] Testing API Data Reception and Display" -ForegroundColor Cyan
    Write-Host

    # Check if configuration files exist
    $configFiles = @("config.json", ".env")
    $missingFiles = @()
    
    foreach ($file in $configFiles) {
        if (-not (Test-Path $file)) {
            $missingFiles += $file
        }
    }
    
    if ($missingFiles.Count -gt 0) {
        Write-Host "Error: Missing configuration files: $($missingFiles -join ', ')" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "[+] Configuration files found" -ForegroundColor Green
    
    # Test 1: Quick connectivity test (5 seconds)
    Write-Host
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Test 1: Quick Connectivity Test (5s)" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host
    
    Write-Host "[*] Starting EcoWatt Device for connectivity test..." -ForegroundColor Cyan
    
    $connectivityJob = Start-Job -ScriptBlock {
        param($exePath)
        & $exePath
    } -ArgumentList (Resolve-Path ".\EcoWattDevice.exe")
    
    Start-Sleep -Seconds 5
    Stop-Job $connectivityJob
    $connectivityOutput = Receive-Job $connectivityJob
    Remove-Job $connectivityJob
    
    Write-Host "--- Connectivity Test Output ---" -ForegroundColor Yellow
    $connectivityOutput | ForEach-Object { Write-Host $_ }
    Write-Host "--- End Connectivity Test ---" -ForegroundColor Yellow
    
    # Test 2: Extended data collection test (15 seconds)
    Write-Host
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Test 2: Extended Data Collection (15s)" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host
    
    Write-Host "[*] Running extended data collection test..." -ForegroundColor Cyan
    Write-Host "[*] This will collect multiple polling cycles to verify data consistency" -ForegroundColor Cyan
    Write-Host
    
    $dataJob = Start-Job -ScriptBlock {
        param($exePath)
        & $exePath
    } -ArgumentList (Resolve-Path ".\EcoWattDevice.exe")
    
    # Monitor job output in real-time for 15 seconds
    $endTime = (Get-Date).AddSeconds(15)
    $dataCollected = @()
    
    while ((Get-Date) -lt $endTime) {
        $output = Receive-Job $dataJob
        if ($output) {
            $output | ForEach-Object { 
                Write-Host $_ 
                $dataCollected += $_
            }
        }
        Start-Sleep -Milliseconds 500
    }
    
    Stop-Job $dataJob
    $remainingOutput = Receive-Job $dataJob
    $dataCollected += $remainingOutput
    Remove-Job $dataJob
    
    # Analysis of collected data
    Write-Host
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Data Analysis Results" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    
    # Look for successful readings
    $successfulReads = $dataCollected | Where-Object { $_ -match "Current Readings" }
    $errorLines = $dataCollected | Where-Object { $_ -match "error|Error|ERROR|failed|Failed" }
    $pollLines = $dataCollected | Where-Object { $_ -match "Total Polls:" }
    
    Write-Host
    Write-Host "[*] Test Results Summary:" -ForegroundColor Cyan
    Write-Host "   - Configuration loading: " -NoNewline
    if ($dataCollected | Where-Object { $_ -match "Configuration loaded successfully" }) {
        Write-Host "SUCCESS" -ForegroundColor Green
    } else {
        Write-Host "FAILED" -ForegroundColor Red
    }
    
    Write-Host "   - Communication test: " -NoNewline
    if ($dataCollected | Where-Object { $_ -match "Communication test successful" }) {
        Write-Host "SUCCESS" -ForegroundColor Green
    } else {
        Write-Host "FAILED" -ForegroundColor Red
    }
    
    Write-Host "   - Data readings display: " -NoNewline
    if ($successfulReads.Count -gt 0) {
        Write-Host "SUCCESS ($($successfulReads.Count) reading blocks)" -ForegroundColor Green
    } else {
        Write-Host "NO DATA RECEIVED" -ForegroundColor Red
    }
    
    Write-Host "   - Error count: " -NoNewline
    if ($errorLines.Count -eq 0) {
        Write-Host "0 (PERFECT)" -ForegroundColor Green
    } elseif ($errorLines.Count -lt 5) {
        Write-Host "$($errorLines.Count) (MINOR ISSUES)" -ForegroundColor Yellow
    } else {
        Write-Host "$($errorLines.Count) (SIGNIFICANT ISSUES)" -ForegroundColor Red
    }
    
    # Display sample data if found
    $voltageReads = $dataCollected | Where-Object { $_ -match "Vac1_L1_Phase_voltage" }
    $currentReads = $dataCollected | Where-Object { $_ -match "Iac1_L1_Phase_current" }
    $powerReads = $dataCollected | Where-Object { $_ -match "Pac_L_Inverter_output_power" }
    
    if ($voltageReads -or $currentReads -or $powerReads) {
        Write-Host
        Write-Host "[+] Sample API Data Received:" -ForegroundColor Green
        if ($voltageReads) { $voltageReads | Select-Object -First 1 | ForEach-Object { Write-Host "   $_" -ForegroundColor Cyan } }
        if ($currentReads) { $currentReads | Select-Object -First 1 | ForEach-Object { Write-Host "   $_" -ForegroundColor Cyan } }
        if ($powerReads) { $powerReads | Select-Object -First 1 | ForEach-Object { Write-Host "   $_" -ForegroundColor Cyan } }
    }
    
    # Check for specific error patterns
    if ($errorLines.Count -gt 0) {
        Write-Host
        Write-Host "[!] Errors Found:" -ForegroundColor Yellow
        $curlErrors = $errorLines | Where-Object { $_ -match "CURL" }
        $jsonErrors = $errorLines | Where-Object { $_ -match "JSON|parse" }
        $httpErrors = $errorLines | Where-Object { $_ -match "HTTP" }
        
        if ($curlErrors) { Write-Host "   - CURL connectivity issues: $($curlErrors.Count)" -ForegroundColor Red }
        if ($jsonErrors) { Write-Host "   - JSON parsing issues: $($jsonErrors.Count)" -ForegroundColor Red }
        if ($httpErrors) { Write-Host "   - HTTP communication issues: $($httpErrors.Count)" -ForegroundColor Red }
        
        Write-Host
        Write-Host "Recent Error Details:" -ForegroundColor Yellow
        $errorLines | Select-Object -Last 3 | ForEach-Object { Write-Host "   $_" -ForegroundColor Red }
    }
    
    # Check log file for additional information
    if (Test-Path "ecoWatt_milestone2.log") {
        Write-Host
        Write-Host "[*] Log file analysis:" -ForegroundColor Cyan
        $logContent = Get-Content "ecoWatt_milestone2.log" -Tail 50
        $logErrors = $logContent | Where-Object { $_ -match "error|Error|ERROR" }
        $logWarnings = $logContent | Where-Object { $_ -match "warning|Warning|WARN" }
        
        Write-Host "   - Recent log entries: $($logContent.Count)" -ForegroundColor White
        Write-Host "   - Errors in log: $($logErrors.Count)" -ForegroundColor $(if ($logErrors.Count -eq 0) { "Green" } else { "Red" })
        Write-Host "   - Warnings in log: $($logWarnings.Count)" -ForegroundColor $(if ($logWarnings.Count -eq 0) { "Green" } else { "Yellow" })
        
        if ($logErrors) {
            Write-Host
            Write-Host "Recent Log Errors:" -ForegroundColor Yellow
            $logErrors | Select-Object -Last 2 | ForEach-Object { Write-Host "   $_" -ForegroundColor Red }
        }
    }
    
    Write-Host
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "API Test Recommendations" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    
    if ($dataCollected | Where-Object { $_ -match "Communication test successful" }) {
        Write-Host "[+] API communication is working correctly" -ForegroundColor Green
        Write-Host "[+] Data is being received from the Inverter SIM" -ForegroundColor Green
        
        if ($successfulReads.Count -gt 1) {
            Write-Host "[+] Multiple polling cycles completed successfully" -ForegroundColor Green
            Write-Host "[+] Real-time data display is functioning properly" -ForegroundColor Green
        }
    } else {
        Write-Host "[!] API communication issues detected" -ForegroundColor Red
        Write-Host "[!] Check network connectivity to: http://20.15.114.131:8080" -ForegroundColor Yellow
        Write-Host "[!] Verify the Inverter SIM service is running" -ForegroundColor Yellow
    }
    
    if ($errorLines.Count -gt 10) {
        Write-Host "[!] High error rate detected - investigate connectivity" -ForegroundColor Red
    } elseif ($errorLines.Count -gt 0) {
        Write-Host "[!] Some errors present but functionality appears working" -ForegroundColor Yellow
    }
    
} catch {
    Write-Host "Error during testing: $_" -ForegroundColor Red
} finally {
    Pop-Location
}

Write-Host
Write-Host "========================================" -ForegroundColor Green
Write-Host "API Data Test Completed!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
