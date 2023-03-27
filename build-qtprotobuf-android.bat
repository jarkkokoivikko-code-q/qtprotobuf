@echo off
setlocal

call build-qtprotobuf-msvc2019_64.bat "%~1" RelWithDebInfo
if %ERRORLEVEL% NEQ 0 goto FAIL

call build-grpc-android.bat "%~1"
if %ERRORLEVEL% NEQ 0 goto FAIL

if exist %cd%\..\openssl\openssl-android-armv7 set OPENSSL_PARAMS=-DOPENSSL_ROOT_DIR=%cd%\..\openssl\openssl-android-armv7

call env-android.cmd
set SOURCE_DIR=%cd%
if "%~1"=="" (set HOST_PATH=%cd%-windows) else (set HOST_PATH=%~1\windows)
if "%~1"=="" (set CMAKE_INSTALL_PREFIX=%cd%-android) else (set CMAKE_INSTALL_PREFIX=%~1\android)

if "%~1"=="" (set BUILD_DIR=%cd%-build-android) else (set BUILD_DIR=%~1%\.build-android)
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_PREFIX_PATH=%QT_ANDROID% -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=%QT_MSVC% -DQT_QMAKE_EXECUTABLE=%QT_ANDROID%\bin\qmake.bat -DCMAKE_TOOLCHAIN_FILE=%CMAKE_TOOLCHAIN_FILE% -DQT_PROTOBUF_HOST_PATH=%HOST_PATH%\lib\cmake -DQT_PROTOBUF_PROTOC_EXECUTABLE=%HOST_PATH%\bin\protoc.exe %OPENSSL_PARAMS% -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH -DQT_PROTOBUF_NATIVE_GRPC_CHANNEL=ON
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

