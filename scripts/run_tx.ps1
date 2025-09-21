param([string]$Config = "config\config.json")
Set-Location "$PSScriptRoot\..\build\qt_deploy"
$env:DDS_TEST_LOOPBACK = "1"
Remove-Item Env:\ALLOW_MULTICAST_TESTS -ErrorAction SilentlyContinue
$env:QT_LOGGING_RULES = "dds.disc=true;dds.net=true"
.\test_discovery_tx.exe --config $Config