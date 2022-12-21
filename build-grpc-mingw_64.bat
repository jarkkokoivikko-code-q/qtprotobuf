@echo off
setlocal

call env-mingw_64.cmd
set TOOLCHAIN_FILE=%cd%\toolchain-mingw_64.cmake
set SOURCE_DIR=%cd%\3rdparty\grpc
set CMAKE_INSTALL_PREFIX=%cd%-mingw_64

set BUILD_DIR=%cd%-build-3rdparty-grpc-mingw_64
mkdir %BUILD_DIR%
pushd %BUILD_DIR%
if not exist CMakeCache.txt (
    cmake %SOURCE_DIR% -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_INSTALL_PREFIX=%CMAKE_INSTALL_PREFIX% -DCMAKE_TOOLCHAIN_FILE=%CMAKE_TOOLCHAIN_FILE% -DOPENSSL_NO_ASM=ON
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

