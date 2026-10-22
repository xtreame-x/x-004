@echo off
setlocal
cd /d "%~dp0"

if not exist WinWidgets.exe (
    if exist build\WinWidgets.exe (
        copy /y build\WinWidgets.exe WinWidgets.exe >nul
    ) else (
        echo [!] WinWidgets.exe not found. Running build.bat first...
        call build.bat
    )
)

echo Launching WinWidgets Desktop Gadgets...
start "" WinWidgets.exe
