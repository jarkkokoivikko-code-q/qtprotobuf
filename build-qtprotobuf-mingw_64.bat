@echo off
setlocal

call build-grpc-mingw_64.bat
if %ERRORLEVEL% NEQ 0 goto FAIL

call env-mingw_64.cmd
set SOURCE_DIR=%cd%
set CMAKE_INSTALL_PREFIX=%SOURCE_DIR%-mingw_64

set BUILD_DIR=%SOURCE_DIR%-build-mingw_64
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_PREFIX_PATH=%QT_MINGW%;%CMAKE_INSTALL_PREFIX% -DCMAKE_TOOLCHAIN_FILE=%CMAKE_TOOLCHAIN_FILE% -DQT_PROTOBUF_NATIVE_GRPC_CHANNEL=ON
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

