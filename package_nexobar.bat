@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "BUILD_DIR=%~1"
if not defined BUILD_DIR set "BUILD_DIR=%PROJECT_DIR%cmake-build-release"

for %%I in ("%BUILD_DIR%") do set "BUILD_DIR=%%~fI"
set "OUTPUT_DIR=%PROJECT_DIR%dist\nexobar"

if not exist "%BUILD_DIR%\cmake_install.cmake" (
    echo ERROR: Release directory not found or not configured: "%BUILD_DIR%"
    echo Usage: %~nx0 [release-build-directory]
    exit /b 1
)

set "CMAKE_EXE="
for /f "usebackq tokens=1,* delims==" %%A in (`findstr /B /C:"CMAKE_COMMAND:INTERNAL=" "%BUILD_DIR%\CMakeCache.txt"`) do set "CMAKE_EXE=%%B"
if not defined CMAKE_EXE for %%I in (cmake.exe) do set "CMAKE_EXE=%%~$PATH:I"
if not defined CMAKE_EXE (
    echo ERROR: CMake could not be found.
    exit /b 1
)

if exist "%OUTPUT_DIR%" "%CMAKE_EXE%" -E remove_directory "%OUTPUT_DIR%"
if errorlevel 1 exit /b 1

"%CMAKE_EXE%" --install "%BUILD_DIR%" --config Release --component NexoBarRuntime --prefix "%OUTPUT_DIR%"
if errorlevel 1 (
    echo ERROR: NexoBar could not be packaged.
    exit /b 1
)

if not exist "%OUTPUT_DIR%\NexoBar.exe" (
    echo ERROR: NexoBar.exe was not copied to the package.
    exit /b 1
)
for /r "%OUTPUT_DIR%\resources" %%F in (*) do if %%~zF EQU 0 (
    echo ERROR: Empty resource file: "%%F"
    exit /b 1
)

echo NexoBar package created in "%OUTPUT_DIR%"
endlocal
