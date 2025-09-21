Set-Location "$PSScriptRoot\..\build\qt_deploy"

# Prepare two RX configs (duplicate the sample config)
Copy-Item .\config\config_broadcast.json .\config\rx1.json -Force
Copy-Item .\config\config_broadcast.json .\config\rx2.json -Force

# Start RX1 and RX2 in separate terminals
Start-Process powershell -ArgumentList '-NoExit','-Command', 'cd "$($pwd)"; $env:DDS_TEST_LOOPBACK="1"; Remove-Item Env:\ALLOW_MULTICAST_TESTS -ErrorAction SilentlyContinue; $env:QT_LOGGING_RULES="dds.disc=true;dds.net=true"; .\test_discovery_rx.exe --config config\rx1.json'
Start-Process powershell -ArgumentList '-NoExit','-Command', 'cd "$($pwd)"; $env:DDS_TEST_LOOPBACK="1"; Remove-Item Env:\ALLOW_MULTICAST_TESTS -ErrorAction SilentlyContinue; $env:QT_LOGGING_RULES="dds.disc=true;dds.net=true"; .\test_discovery_rx.exe --config config\rx2.json'

# Run TX in current window
$env:DDS_TEST_LOOPBACK = "1"
Remove-Item Env:\ALLOW_MULTICAST_TESTS -ErrorAction SilentlyContinue
$env:QT_LOGGING_RULES = "dds.disc=true;dds.net=true"
.\test_discovery_tx.exe --config config\config.json