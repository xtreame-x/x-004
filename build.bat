@echo off
setlocal
echo =======================================================
echo          Building WinWidgets (C++ / Win32)
echo =======================================================

cd /d "%~dp0"

if not exist build mkdir build
cd build

cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [CMake configuration failed, falling back to direct g++ build...]
    cd ..
    goto DIRECT_BUILD
)

ninja
if %ERRORLEVEL% NEQ 0 (
    echo [Ninja build failed, falling back to direct g++ build...]
    cd ..
    goto DIRECT_BUILD
)

echo.
echo =======================================================
echo Build Successful! Executable is at build\WinWidgets.exe
echo =======================================================
copy /y WinWidgets.exe ..\WinWidgets.exe >nul
cd ..
exit /b 0

:DIRECT_BUILD
echo Compiling resource...
windres -i src\resources\resource.rc -o build\resource.o -I src\resources

echo Compiling C++ files...
g++ -std=c++20 -O2 -mwindows -municode -DUNICODE -D_UNICODE ^
    src\main.cpp ^
    src\app.cpp ^
    src\graphics\d2d_context.cpp ^
    src\utils\system_stats.cpp ^
    src\utils\http_client.cpp ^
    src\widgets\widget_base.cpp ^
    src\widgets\widget_clock.cpp ^
    src\widgets\widget_system.cpp ^
    src\widgets\widget_weather.cpp ^
    src\widgets\widget_calendar.cpp ^
    src\widgets\widget_tasks.cpp ^
    src\widgets\widget_news.cpp ^
    src\widgets\widget_hub.cpp ^
    build\resource.o ^
    -ld2d1 -ldwrite -lwindowscodecs -lole32 -lshlwapi -lcomctl32 -lwinhttp -ldwmapi -lgdi32 -luser32 -lshell32 ^
    -o WinWidgets.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo =======================================================
    echo Build Successful! Executable is WinWidgets.exe
    echo =======================================================
    exit /b 0
) else (
    echo.
    echo [!] Build Failed!
    exit /b 1
)
