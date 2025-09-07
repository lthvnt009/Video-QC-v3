@echo off
setlocal

:: ============================================================================
:: ==   SCRIPT BIEN DICH PHIEN BAN RELEASE CHO VIDEO QC TOOL CONTROL PANEL   ==
:: ============================================================================
:: ==  Script nay se tu dong tim moi truong Visual Studio 2022, xoa build    ==
:: ==  cu, cau hinh lai CMake, bien dich va dong goi san pham hoan chinh.     ==
:: ============================================================================

echo [INFO] Bat dau qua trinh bien dich va dong goi...
echo.

REM --- Buoc 1: Thiet lap moi truong Visual Studio 2022 ---
echo [BUOC 1/6] Thiet lap moi truong Visual Studio 2022...

set "VS_PATH_COMMUNITY=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
set "VS_PATH_PRO=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
set "VS_PATH_ENT=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"

set "VS_DEV_CMD="
if exist "%VS_PATH_ENT%" set "VS_DEV_CMD=%VS_PATH_ENT%"
if exist "%VS_PATH_PRO%" set "VS_DEV_CMD=%VS_PATH_PRO%"
if exist "%VS_PATH_COMMUNITY%" set "VS_DEV_CMD=%VS_PATH_COMMUNITY%"

if not defined VS_DEV_CMD (
    echo [LOI] Khong tim thay moi truong Visual Studio 2022 Developer Command Prompt.
    goto :error
)
echo      -> Da tim thay moi truong tai: "%VS_DEV_CMD%"
call "%VS_DEV_CMD%" -arch=amd64 > nul
echo      -> Moi truong da san sang.
echo.


REM --- Cau hinh duong dan ---
set "PROJECT_NAME=VideoQCTool_v2.0"
set "QT_PATH=C:\Qt\6.9.1\msvc2022_64"
set "BUILD_DIR=build"
set "DEPLOY_DIR=Video QC Tool"


REM --- Buoc 2: Don dep va kiem tra ---
echo [BUOC 2/6] Don dep va kiem tra duong dan...

if exist "%BUILD_DIR%" (
    echo      -> Xoa thu muc build cu...
    rmdir /s /q "%BUILD_DIR%"
)
if exist "%DEPLOY_DIR%" (
    echo      -> Xoa thu muc dong goi cu...
    rmdir /s /q "%DEPLOY_DIR%"
)

if not exist "%QT_PATH%" (
    echo [LOI] Khong tim thay thu muc Qt tai: %QT_PATH%
    goto :error
)
echo      -> Kiem tra hoan tat.
echo.


REM --- Buoc 3: Cau hinh CMake ---
echo [BUOC 3/6] Tao thu muc build va cau hinh CMake...
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_PATH%" ..

if %errorlevel% neq 0 (
    echo [LOI] Qua trinh cau hinh CMake that bai.
    goto :error
)
echo      -> Cau hinh CMake thanh cong.
echo.

REM --- Buoc 4: Bien dich (Release) ---
echo [BUOC 4/6] Bien dich du an (che do Release)...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo [LOI] Qua trinh bien dich that bai.
    goto :error
)
echo      -> Bien dich thanh cong.
echo.
cd ..

REM --- Buoc 5: Chuan bi thu muc dong goi ---
echo [BUOC 5/6] Chuan bi thu muc dong goi...
mkdir "%DEPLOY_DIR%"
echo      -> Sao chep file .exe...
copy "%BUILD_DIR%\Release\%PROJECT_NAME%.exe" "%DEPLOY_DIR%\" > nul
echo.

REM --- Buoc 6: Dong goi thu vien phu thuoc ---
echo [BUOC 6/6] Dong goi cac thu vien can thiet...
echo      -> Dong goi thu vien Qt...
call "%QT_PATH%\bin\windeployqt.exe" --release --no-translations --no-opengl-sw "%DEPLOY_DIR%\%PROJECT_NAME%.exe"
if %errorlevel% neq 0 (
    echo [LOI] windeployqt that bai.
    goto :error
)
if exist "ffmpeg" (
    echo      -> Dong goi thu vien FFmpeg...
    xcopy "ffmpeg" "%DEPLOY_DIR%\ffmpeg\" /s /e /i /y > nul
)
echo      -> Dong goi hoan tat.
echo.

goto :success

:error
echo.
echo ===================================
echo ==      BUILD THAT BAI           ==
echo ===================================
echo Vui long kiem tra cac thong bao loi o tren.
goto :end

:success
echo.
echo ===================================
echo ==      HOAN THANH!              ==
echo ===================================
echo San pham da san sang trong thu muc: "%DEPLOY_DIR%"
echo.

:end
pause
endlocal

