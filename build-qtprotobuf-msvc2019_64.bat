@echo off
setlocal

call build-grpc-msvc2019_64.bat
if %ERRORLEVEL% NEQ 0 goto FAIL

call env-msvc2019_64.cmd
set SOURCE_DIR=%cd%
set CMAKE_INSTALL_PREFIX=%SOURCE_DIR%-msvc2019_64

if exist %cd%\..\openssl\openssl-win64 set OPENSSL_PARAMS=-DOPENSSL_ROOT_DIR=%cd%\..\openssl\openssl-win64

set BUILD_DIR=%SOURCE_DIR%-build-msvc2019_64-Debug
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_PREFIX_PATH=%QT_MSVC%;%CMAKE_INSTALL_PREFIX% %OPENSSL_PARAMS%
    if %ERRORLEVEL% NEQ 0 goto FAIL
)
cmake --build .
if %ERRORLEVEL% NEQ 0 goto FAIL
cmake --install .
if %ERRORLEVEL% NEQ 0 goto FAIL
popd

set BUILD_DIR=%SOURCE_DIR%-build-msvc2019_64-RelWithDebInfo
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_PREFIX_PATH=%QT_MSVC%;%CMAKE_INSTALL_PREFIX% %OPENSSL_PARAMS%
    if %ERRORLEVEL% NEQ 0 goto FAIL
)
cmake --build .
if %ERRORLEVEL% NEQ 0 goto FAIL
cmake --install .
if %ERRORLEVEL% NEQ 0 goto FAIL
popd

goto SUCCESS

:FAIL
popd
echo Build failed
pause
goto END

:SUCCESS
echo Build complete

:END

