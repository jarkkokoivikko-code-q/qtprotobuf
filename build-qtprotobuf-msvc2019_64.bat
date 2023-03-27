@echo off
setlocal

call build-grpc-msvc2019_64.bat "%~1" %~2
if %ERRORLEVEL% NEQ 0 goto FAIL

call env-msvc2019_64.cmd
set SOURCE_DIR=%cd%
if "%~1"=="" (set CMAKE_INSTALL_PREFIX=%cd%-windows) else (set CMAKE_INSTALL_PREFIX=%~1\windows)
if exist %cd%\..\openssl\openssl-win64 set OPENSSL_PARAMS=-DOPENSSL_ROOT_DIR=%cd%\..\openssl\openssl-win64

if "%~2"=="" goto BUILD_DEBUG
if /i "%~2"=="Debug" goto BUILD_DEBUG
goto SKIP_DEBUG

:BUILD_DEBUG
if "%~1"=="" (set BUILD_DIR=%cd%-build-msvc2019_64-Debug) else (set BUILD_DIR=%~1%\.build-windows-debug)
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_PREFIX_PATH=%QT_MSVC%;%CMAKE_INSTALL_PREFIX% %OPENSSL_PARAMS% -DQT_PROTOBUF_NATIVE_GRPC_CHANNEL=ON
    if %ERRORLEVEL% NEQ 0 goto FAIL
)
cmake --build .
if %ERRORLEVEL% NEQ 0 goto FAIL
cmake --install .
if %ERRORLEVEL% NEQ 0 goto FAIL
popd
:SKIP_DEBUG

if "%~2"=="" goto BUILD_RELEASE
if /i "%~2"=="RelWithDebInfo" goto BUILD_RELEASE
goto SKIP_RELEASE

:BUILD_RELEASE
if "%~1"=="" (set BUILD_DIR=%cd%-build-msvc2019_64-RelWithDebInfo) else (set BUILD_DIR=%~1%\.build-windows-release)
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_PREFIX_PATH=%QT_MSVC%;%CMAKE_INSTALL_PREFIX% %OPENSSL_PARAMS% -DQT_PROTOBUF_NATIVE_GRPC_CHANNEL=ON
    if %ERRORLEVEL% NEQ 0 goto FAIL
)
cmake --build .
if %ERRORLEVEL% NEQ 0 goto FAIL
cmake --install .
if %ERRORLEVEL% NEQ 0 goto FAIL
popd
:SKIP_RELEASE

goto SUCCESS

:FAIL
popd
echo Build failed
goto END

:SUCCESS
echo Build complete

:END

