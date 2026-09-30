@echo off
if not exist build mkdir build
C:\mingw64\bin\gcc.exe -std=c11 -O2 -Wall -Wextra lab1.c -o build\lab1.exe
if errorlevel 1 exit /b 1
echo Built build\lab1.exe
