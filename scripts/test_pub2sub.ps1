# DDS Mini-Bus Integration Test Runner (Windows)
# Tests 1 publisher -> 2 subscribers with reliable QoS

param(
    [string]$BinaryPath = "..\build\dds_mini_bus.exe",
    [string]$ConfigDir = "..\config",
    [string]$Topic = "sensor/temperature",
    [int]$MessageCount = 5,
    [int]$IntervalMs = 200
)

# Windows Qt/MinGW PATH hotfix (belt-and-suspenders)
$env:PATH = "C:\Qt\6.9.2\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"

Write-Host "=== DDS Mini-Bus Integration Test ===" -ForegroundColor Cyan
Write-Host "Binary: $BinaryPath" -ForegroundColor Gray
Write-Host "Config: $ConfigDir" -ForegroundColor Gray
Write-Host "Topic: $Topic" -ForegroundColor Gray
Write-Host "Messages: $MessageCount" -ForegroundColor Gray
Write-Host "Interval: ${IntervalMs}ms" -ForegroundColor Gray
Write-Host ""

# Check if binary exists
if (!(Test-Path $BinaryPath)) {
    Write-Error "Binary not found at $BinaryPath"
    Write-Host "Please build the project first: mkdir build; cd build; cmake ..; cmake --build ."
    exit 1
}

# Check if configs exist
$rxConfig = Join-Path $ConfigDir "config_rx.json"
$txConfig = Join-Path $ConfigDir "config_tx.json"

if (!(Test-Path $rxConfig)) {
    Write-Error "Receiver config not found at $rxConfig"
    exit 1
}

if (!(Test-Path $txConfig)) {
    Write-Error "Sender config not found at $txConfig"
    exit 1
}

Write-Host "Starting Subscriber 1..." -ForegroundColor Yellow
$sub1 = Start-Process -FilePath $BinaryPath -ArgumentList "--role subscriber --topic $Topic --config $rxConfig --log-level info" -PassThru -NoNewWindow

Write-Host "Starting Subscriber 2..." -ForegroundColor Yellow
$sub2 = Start-Process -FilePath $BinaryPath -ArgumentList "--role subscriber --topic $Topic --config $rxConfig --log-level info" -PassThru -NoNewWindow

# Wait for discovery
Write-Host "Waiting for discovery..." -ForegroundColor Gray
Start-Sleep -Seconds 3

Write-Host "Starting Publisher..." -ForegroundColor Yellow
$pub = Start-Process -FilePath $BinaryPath -ArgumentList "--role sender --topic $Topic --qos reliable --count $MessageCount --interval-ms $IntervalMs --payload {`"value`":23.5,`"unit`":`"C`"} --config $txConfig --log-level debug" -PassThru -NoNewWindow

# Wait for completion
Write-Host "Waiting for test completion..." -ForegroundColor Gray
$pub.WaitForExit(15000)
$sub1.WaitForExit(5000)
$sub2.WaitForExit(5000)

# Check exit codes
$pubExit = $pub.ExitCode
$sub1Exit = $sub1.ExitCode
$sub2Exit = $sub2.ExitCode

Write-Host ""
Write-Host "=== Test Results ===" -ForegroundColor Cyan
Write-Host "Publisher exit code: $pubExit"
Write-Host "Subscriber 1 exit code: $sub1Exit"
Write-Host "Subscriber 2 exit code: $sub2Exit"

if ($pubExit -eq 0 -and $sub1Exit -eq 0 -and $sub2Exit -eq 0) {
    Write-Host "✓ All processes completed successfully" -ForegroundColor Green
} else {
    Write-Host "✗ Some processes failed" -ForegroundColor Red
}

Write-Host ""
Write-Host "Test completed. Check logs above for detailed results." -ForegroundColor Gray