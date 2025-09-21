@echo off
REM DDS Mini-Bus Receiver Demo Script (Windows)
REM Usage: run_rx.bat [TOPIC] [CONFIG]

if "%BIN%"=="" set BIN=..\build\dds_mini_bus.exe
if "%CFG%"=="" set CFG=..\config\config_rx.json
if "%ROLE%"=="" set ROLE=subscriber
if "%TOPIC%"=="" set TOPIC=sensor/temperature

if not exist "%BIN%" (
    echo Error: Binary not found at %BIN%
    echo Please build the project first: mkdir build ^& cd build ^& cmake .. ^& cmake --build .
    exit /b 1
)

if not exist "%CFG%" (
    echo Error: Config not found at %CFG%
    exit /b 1
)

echo Starting DDS receiver...
echo   Binary: %BIN%
echo   Config: %CFG%
echo   Role: %ROLE%
echo   Topic: %TOPIC%
echo.

"%BIN%" --role %ROLE% --topic %TOPIC% --config %CFG% --log-level info