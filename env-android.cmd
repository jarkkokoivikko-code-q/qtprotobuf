@ECHO OFF

call env-common.cmd

set CMAKE_TOOLCHAIN_FILE=%LOCALAPPDATA%\Android\Sdk\ndk\22.1.7171670\build\cmake\android.toolchain.cmake

set PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem
set PATH=%PATH%;C:\Qt\Tools\mingw1120_64\bin
set PATH=%PATH%;C:\Qt\Tools\CMake_64\bin
set PATH=%PATH%;C:\Qt\Tools\Ninja
