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
if (-not $qtPath -or -not $mingwPath) {
    Write-Host "Qt or MinGW not found. Please set QT_ROOT and MINGW_ROOT or install to C:\Qt"
    exit 1
}
$env:PATH = (Join-Path $qtPath "bin") + ";" + $mingwPath + ";" + $env:PATH

# Build in separate dir
$buildDir = "build-tests"
if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir }
New-Item -ItemType Directory -Path $buildDir | Out-Null
Set-Location $buildDir

cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$qtPath\lib\cmake"
if ($LASTEXITCODE -ne 0) { exit 1 }

mingw32-make -j
if ($LASTEXITCODE -ne 0) { exit 1 }

ctest -C Debug --output-on-failure
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "All tests passed!"