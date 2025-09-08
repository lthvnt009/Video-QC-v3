@echo off
setlocal

:: ============================================================================
:: ==       RELEASE BUILD SCRIPT FOR VIDEO QC TOOL CONTROL PANEL           ==
:: ============================================================================
:: == This script automates finding the VS 2022 environment, cleaning,       ==
:: == configuring, building, and deploying the final application.            ==
:: ============================================================================

echo [INFO] Starting build and deployment process...
echo.

REM --- Step 1: Setup Visual Studio 2022 Environment ---
echo [STEP 1/5] Setting up Visual Studio 2022 environment...

set "VS_PATH_COMMUNITY=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
set "VS_PATH_PRO=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
set "VS_PATH_ENT=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"

set "VS_DEV_CMD="
if exist "%VS_PATH_ENT%" set "VS_DEV_CMD=%VS_PATH_ENT%"
if exist "%VS_PATH_PRO%" set "VS_DEV_CMD=%VS_PATH_PRO%"
if exist "%VS_PATH_COMMUNITY%" set "VS_DEV_CMD=%VS_PATH_COMMUNITY%"

if not defined VS_DEV_CMD (
    echo [ERROR] Visual Studio 2022 Developer Command Prompt not found.
    goto :error
)
echo      - Found environment at: "%VS_DEV_CMD%"
call "%VS_DEV_CMD%" -arch=amd64 > nul
echo      - Environment is ready.
echo.


REM --- Path Configuration ---
set "PROJECT_NAME=VideoQCTool_v2.2"
set "QT_PATH=C:\Qt\6.9.1\msvc2022_64"
set "BUILD_DIR=build"
set "DEPLOY_DIR=Video QC Tool"


REM --- Step 2: Cleanup and Checks ---
echo [STEP 2/5] Cleaning up and checking paths...

if exist "%BUILD_DIR%" (
    echo      - Deleting old build directory...
    rmdir /s /q "%BUILD_DIR%"
)
if exist "%DEPLOY_DIR%" (
    echo      - Deleting old deployment directory...
    rmdir /s /q "%DEPLOY_DIR%"
)

if not exist "%QT_PATH%" (
    echo [ERROR] Qt directory not found at: %QT_PATH%
    goto :error
)
echo      - Checks completed.
echo.


REM --- Step 3 & 4: CMake Configuration and Build ---
echo [STEP 3-4/5] Creating build directory, configuring and building...
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

echo      - Running CMake configuration...
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_PATH%" ..
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    goto :error
)
echo      - Building the project (Release mode)...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build process failed.
    cd ..
    goto :error
)
cd ..
echo      - Configuration and build successful.
echo.

REM --- Step 5: Application Deployment ---
echo [STEP 5/5] Preparing directory and deploying libraries...
mkdir "%DEPLOY_DIR%"
echo      - Copying executable file...
copy "%BUILD_DIR%\Release\%PROJECT_NAME%.exe" "%DEPLOY_DIR%\" > nul

echo      - Deploying Qt libraries...
call "%QT_PATH%\bin\windeployqt.exe" --release --no-translations --no-opengl-sw "%DEPLOY_DIR%\%PROJECT_NAME%.exe"
if %errorlevel% neq 0 (
    echo [ERROR] windeployqt failed.
    goto :error
)
echo      - Deployment finished.
echo.

goto :success

:error
echo.
echo ===================================
echo ==         BUILD FAILED          ==
echo ===================================
echo Please check the error messages above.
goto :end

:success
echo.
echo ===================================
echo ==         SUCCESS!              ==
echo ===================================
echo The application is ready in the folder: "%DEPLOY_DIR%"
echo.

:end
pause
endlocal

