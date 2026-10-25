@echo off
setlocal
where g++ >nul 2>nul
if errorlevel 1 (
  echo [!] g++ was not found on PATH.
  exit /b 1
)

echo Building Obsidian Shell with Live Data...
g++ -std=c++17 -O2 -s -mwindows -static UI_shell.cpp -o UI_shell.exe ^
    -ld2d1 -ldwrite -luser32 -lgdi32 -lwinhttp -lwlanapi -lbthprops
if errorlevel 1 (
  echo [!] Build failed.
  exit /b 1
)
echo [ok] UI_shell.exe built successfully.
