function Find-QtPath {
    if ($env:QT_ROOT) { return $env:QT_ROOT }
    $qtBase = "C:\Qt"
    if (Test-Path $qtBase) {
        $versions = Get-ChildItem $qtBase -Directory | Where-Object { $_.Name -match '^6\.' } | Sort-Object -Property Name -Descending
        if ($versions) {
            $qtPath = Join-Path $qtBase $versions[0].Name "mingw_64"
            if (Test-Path (Join-Path $qtPath "bin\Qt6Core.dll")) { return $qtPath }
        }
    }
    return $null
}

function Find-MinGWPath {
    if ($env:MINGW_ROOT) { return $env:MINGW_ROOT }
    $mingwBase = "C:\Qt\Tools"
    if (Test-Path $mingwBase) {
        $mingwDirs = Get-ChildItem $mingwBase -Directory | Where-Object { $_.Name -match '^mingw' } | Sort-Object -Property Name -Descending
        if ($mingwDirs) {
            $mingwPath = Join-Path $mingwBase $mingwDirs[0].Name "bin"
            if (Test-Path (Join-Path $mingwPath "mingw32-make.exe")) { return $mingwPath }
        }
    }
    return $null
}

Set-Location $PSScriptRoot\..
$qtPath = Find-QtPath
$mingwPath = Find-MinGWPath
$env:PATH = (Join-Path $qtPath "bin") + ";" + $mingwPath + ";" + $env:PATH
.\build\test_discovery_rx.exe --config .\config.json