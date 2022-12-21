@ECHO OFF

call env-common.cmd

set PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem
set PATH=%PATH%;C:\Qt\Tools\CMake_64\bin
set PATH=%PATH%;C:\Qt\Tools\Ninja

REM Set up \Microsoft Visual Studio 2019, where <arch> is \c amd64, \c x86, etc.
CALL "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
