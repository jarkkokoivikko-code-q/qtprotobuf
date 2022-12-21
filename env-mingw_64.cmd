@ECHO OFF

call env-common.cmd

set CMAKE_TOOLCHAIN_FILE=%cd%\toolchain-mingw_64.cmake

set PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem
set PATH=%PATH%;C:\Qt\Tools\mingw1120_64\bin
set PATH=%PATH%;C:\Qt\Tools\CMake_64\bin
set PATH=%PATH%;C:\Qt\Tools\Ninja

REM Link statically so that runtime c++ libraries do not need to be in the path
set CXXFLAGS=-static
