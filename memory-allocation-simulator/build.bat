@echo off
REM Build script for Windows (needs gcc from MinGW-w64 / MSYS2 / Code::Blocks)
gcc -Wall -Wextra -std=c99 -O2 -o memsim.exe main.c allocator.c
if errorlevel 1 (
    echo.
    echo Build FAILED. Is gcc installed and on your PATH?
    exit /b 1
)
echo Build OK. Run it with:  memsim.exe
