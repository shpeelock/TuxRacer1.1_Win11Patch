@echo off
echo ================================================================
echo Building smpeg.dll with DEF file
echo ================================================================
echo.

set PATH=C:\MinGW\bin;%PATH%

echo Compiling...
gcc -shared -o smpeg.dll smpeg_wrapper.c smpeg.def -static-libgcc -static -O2 -s -Wl,--kill-at

if %errorlevel% neq 0 (
    echo.
    echo BUILD FAILED!
    pause
    exit /b 1
)

echo.
echo ================================================================
echo BUILD SUCCESSFUL!
echo ================================================================
echo.
pause