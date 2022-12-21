@echo off
setlocal

call env-msvc2019_64.cmd
set SOURCE_DIR=%cd%\3rdparty\grpc
set CMAKE_INSTALL_PREFIX=%cd%-msvc2019_64

if exist %cd%\..\openssl\openssl-win64 set OPENSSL_PARAMS=-DgRPC_SSL_PROVIDER=package -DOPENSSL_ROOT_DIR=%cd%\..\openssl\openssl-win64

set BUILD_DIR=%cd%-build-3rdparty-grpc-msvc2019_64-Debug
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_DEBUG_POSTFIX=d %OPENSSL_PARAMS%
    if %ERRORLEVEL% NEQ 0 goto FAIL
)
cmake --build . --parallel
if %ERRORLEVEL% NEQ 0 goto FAIL
cmake --install .
if %ERRORLEVEL% NEQ 0 goto FAIL
popd

set BUILD_DIR=%cd%-build-3rdparty-grpc-msvc2019_64-RelWithDebInfo
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_DEBUG_POSTFIX=d %OPENSSL_PARAMS%
    if %ERRORLEVEL% NEQ 0 goto FAIL
)
cmake --build . --parallel
if %ERRORLEVEL% NEQ 0 goto FAIL
cmake --install .
if %ERRORLEVEL% NEQ 0 goto FAIL
popd

goto SUCCESS

:FAIL
popd
echo Build failed
goto END

:SUCCESS
echo Build complete

:END