@echo off
REM DDS Mini-Bus Sender Demo Script (Windows)
REM Usage: run_tx.bat [TOPIC] [QOS] [COUNT] [INTERVAL] [PAYLOAD]

if "%BIN%"=="" set BIN=..\build\dds_mini_bus.exe
if "%CFG%"=="" set CFG=..\config\config_tx.json
if "%TOPIC%"=="" set TOPIC=sensor/temperature
if "%QOS%"=="" set QOS=reliable
if "%COUNT%"=="" set COUNT=5
if "%INTERVAL%"=="" set INTERVAL=500
if "%PAYLOAD%"=="" set PAYLOAD={"value": 23.5, "unit": "C"}

if not exist "%BIN%" (
    echo Error: Binary not found at %BIN%
    echo Please build the project first: mkdir build ^& cd build ^& cmake .. ^& cmake --build .
    exit /b 1
)

if not exist "%CFG%" (
    echo Error: Config not found at %CFG%
    exit /b 1
)

echo Starting DDS sender...
echo   Binary: %BIN%
echo   Config: %CFG%
echo   Topic: %TOPIC%
echo   QoS: %QOS%
echo   Count: %COUNT%
echo   Interval: %INTERVAL%ms
echo   Payload: %PAYLOAD%
echo.

"%BIN%" --role sender --topic %TOPIC% --qos %QOS% --count %COUNT% --interval-ms %INTERVAL% --payload %PAYLOAD% --config %CFG% --log-level debug