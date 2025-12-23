@echo off
REM Minimal build script for Chrome Password Decryptor
REM Optimized for size: target ≤100KB

setlocal

echo Building minimal Chrome password decryptor...
echo.

REM Check for MSVC environment
if not defined DevEnvDir (
    echo ERROR: Must run from Developer Command Prompt for VS
    exit /b 1
)

echo Target Architecture: %VSCMD_ARG_TGT_ARCH%
echo.

REM Create output directory
if not exist "minimal\build" mkdir "minimal\build"

REM Compile with maximum size optimization
REM /O1 - Minimize size
REM /MT - Static CRT
REM /GS- - Disable security checks
REM /Gy - Enable function-level linking
REM /GL - Whole program optimization
REM /nologo - No banner
REM /W3 - Warning level 3
REM /Isrc - Include src directory

echo Compiling...
cl.exe /nologo /O1 /MT /GS- /Gy /GL /W3 ^
    /Iminimal\src ^
    minimal\src\main.cpp ^
    /link /NOLOGO /SUBSYSTEM:CONSOLE ^
    /LTCG /OPT:REF /OPT:ICF ^
    /ENTRY:mainCRTStartup ^
    /DYNAMICBASE /NXCOMPAT ^
    /OUT:minimal\build\chrome_decrypt.exe ^
    crypt32.lib bcrypt.lib shell32.lib kernel32.lib user32.lib

if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo.
echo Build successful!
echo.

REM Check file size
for %%A in (minimal\build\chrome_decrypt.exe) do set size=%%~zA

echo Executable: minimal\build\chrome_decrypt.exe
echo Size: %size% bytes

REM Convert to KB
set /a size_kb=%size% / 1024
echo Size: %size_kb% KB

if %size_kb% LEQ 100 (
    echo SUCCESS: Size is within 100KB limit!
) else (
    echo WARNING: Size exceeds 100KB limit
)

echo.
echo To run: minimal\build\chrome_decrypt.exe

endlocal
