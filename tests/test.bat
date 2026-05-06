@echo off
setlocal EnableDelayedExpansion

:: AEGIS GCS — Test Runner for Windows
:: Usage: test.bat [QtPrefixPath]
::   If no argument given, tries QT_DIR env var, then common Qt paths.
::   Run this script from the tests directory (or double-click it here).

call :main %*
if errorlevel 1 (
    echo.
    echo Tests FAILED.
    echo Press any key to exit...
    pause > nul
    exit /b 1
)
echo.
echo Tests PASSED.
echo Press any key to exit...
pause > nul
exit /b 0

:: ============================================================
:: MAIN
:: ============================================================
:main

echo ============================================
echo  AEGIS GCS — Build ^& Run Tests
echo ============================================
echo.

:: --- 1. Detect VS environment (if not already set) ---
where cl > nul 2> nul
if errorlevel 1 (
    echo Detecting Visual Studio 2022 environment...
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "VS_PATH=%%i"
        )
    )
    if not defined VS_PATH (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
        if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional"
        if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise"
    )
    if not defined VS_PATH (
        echo ERROR: Visual Studio 2022 not found.
        echo Please run this script from a "Developer Command Prompt for VS 2022" instead.
        exit /b 1
    )
    echo Initializing VS 2022 environment from: !VS_PATH!
    call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x64 > nul
    if errorlevel 1 (
        echo ERROR: Failed to initialize VS environment.
        exit /b 1
    )
    echo VS environment ready.
) else (
    echo VS environment already active.
)
echo.

:: --- 2. Determine Qt prefix path ---
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
)
:found_qt

if not defined QT_PREFIX (
    echo ERROR: Could not find Qt installation.
    echo Please provide the Qt prefix path as an argument or set QT_DIR environment variable.
    echo Example: test.bat "C:\Qt\6.8.2\msvc2022_64"
    exit /b 1
)

echo Using Qt prefix: %QT_PREFIX%
echo.

:: --- 3. Clean old test build artifacts ---
echo Cleaning old test build artifacts...
if exist "build" rmdir /s /q "build"
echo Clean complete.
echo.

:: --- 4. Configure CMake ---
echo Configuring CMake (Debug, Tests ON)...
cmake -B build -S .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH="%QT_PREFIX%" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DAEGIS_BUILD_TESTS=ON

if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    exit /b 1
)
echo Configuration complete.
echo.

:: --- 5. Build ---
echo Building tests (Debug)...
cmake --build build --config Debug --parallel

if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)
echo Build complete.
echo.

:: --- 6. Run tests ---
echo Running tests...
cd build
ctest -C Debug --output-on-failure
set "TEST_RESULT=%errorlevel%"
cd ..

if %TEST_RESULT% neq 0 (
    echo.
    echo ============================================
    echo  TEST SUMMARY: FAILURE
    echo ============================================
    exit /b 1
)

echo.
echo ============================================
echo  TEST SUMMARY: ALL PASSED
echo ============================================

exit /b 0
