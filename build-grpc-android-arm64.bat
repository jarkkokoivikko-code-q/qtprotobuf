@echo off
setlocal

call env-android.cmd
set SOURCE_DIR=%cd%\3rdparty\grpc
if "%~1"=="" (set HOST_PATH=%cd%-windows) else (set HOST_PATH=%~1\windows)
if "%~1"=="" (set CMAKE_INSTALL_PREFIX=%cd%-android-arm64) else (set CMAKE_INSTALL_PREFIX=%~1\android-arm64)

if exist %cd%\..\openssl\openssl-android-arm64 set OPENSSL_PARAMS=-DgRPC_SSL_PROVIDER=package -DOPENSSL_ROOT_DIR=%cd%\..\openssl\openssl-android-arm64

if "%~1"=="" (set BUILD_DIR=%cd%-build-grpc-android-arm64) else (set BUILD_DIR=%~1%\.build-grpc-android-arm64)
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_TOOLCHAIN_FILE=%CMAKE_TOOLCHAIN_FILE% -DANDROID_ABI=arm64-v8a %OPENSSL_PARAMS% -D_gRPC_PROTOBUF_PROTOC_EXECUTABLE=%HOST_PATH%\bin\protoc.exe -D_gRPC_CPP_PLUGIN=%HOST_PATH%\bin\grpc_cpp_plugin.exe -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH
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

