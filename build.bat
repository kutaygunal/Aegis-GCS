@echo off
setlocal EnableDelayedExpansion

:: AEGIS GCS — Clean Release Build Script for Windows
:: Usage: build.bat [QtPrefixPath]
::   If no argument given, tries QT_DIR env var, then common Qt paths.

echo ============================================
echo  AEGIS GCS — Clean Release Build
echo ============================================
echo.

:: --- 1. Determine Qt prefix path ---
set "QT_PREFIX=%~1"
if not defined QT_PREFIX set "QT_PREFIX=%QT_DIR%"

if not defined QT_PREFIX (
    echo No Qt prefix path provided. Probing common locations...
    for %%P in (
        "C:\Qt\6.8.3\msvc2022_64"
        "C:\Qt\6.8.2\msvc2022_64"
        "C:\Qt\6.8.1\msvc2022_64"
        "C:\Qt\6.8.0\msvc2022_64"
        "C:\Qt\6.7.3\msvc2022_64"
        "C:\Qt\6.7.2\msvc2022_64"
        "C:\Qt\6.6.3\msvc2022_64"
        "C:\Qt\6.6.2\msvc2022_64"
        "C:\Qt\6.5.3\msvc2022_64"
    ) do (
        if exist "%%~P\bin\qmake.exe" (
            set "QT_PREFIX=%%~P"
            echo Found Qt at %%~P
            goto :found_qt
        )
    )
    :found_qt
)

if not defined QT_PREFIX (
    echo ERROR: Could not find Qt installation.
    echo Please provide the Qt prefix path as an argument or set QT_DIR environment variable.
    echo Example: build.bat "C:\Qt\6.8.2\msvc2022_64"
    exit /b 1
)

echo Using Qt prefix: %QT_PREFIX%
echo.

:: --- 2. Clean old build artifacts ---
echo Cleaning old build artifacts...
if exist "build"          rmdir /s /q "build"
if exist "build_cesium"    rmdir /s /q "build_cesium"
if exist "build_check"     rmdir /s /q "build_check"
if exist "aegis.log"       del /f /q "aegis.log"
if exist "aegis.pdb"       del /f /q "aegis.pdb"
if exist "aegis.exe"       del /f /q "aegis.exe"
echo Clean complete.
echo.

:: --- 3. Configure CMake ---
echo Configuring CMake (Release, Tests OFF)...
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH="%QT_PREFIX%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DAEGIS_BUILD_TESTS=OFF

if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)
echo Configuration complete.
echo.

:: --- 4. Build ---
echo Building Release...
cmake --build build --config Release --parallel

if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)
echo Build complete.
echo.

:: --- 5. Summarize ---
echo ============================================
echo  Build Summary
echo ============================================
echo Output dir:  build\Release\
echo Executable:  build\Release\aegis.exe

if exist "build\Release\aegis.exe" (
    echo Status:      SUCCESS
) else (
    echo Status:      FAILED — aegis.exe not found
    exit /b 1
)

echo.
echo You can run the application with:
echo   .\build\Release\aegis.exe

endlocal
