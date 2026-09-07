@echo off
setlocal

if not exist build mkdir build

cmake -S . -B build -A x64
if errorlevel 1 exit /b 1

cmake --build build --config Release
if errorlevel 1 exit /b 1

