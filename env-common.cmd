@echo off

if "%QT_ROOT%"=="" (
    set QT_ROOT=C:\Qt\6.3.1
    if exist env-custom.cmd call env-custom.cmd
)

set QT_MSVC=%QT_ROOT%\msvc2019_64
set QT_MINGW=%QT_ROOT%\mingw_64
set QT_ANDROID=%QT_ROOT%\android_armv7

REM Clean environent variables which interfere cross compilation
set ANDROID_HOME=
set ANDROID_NDK_HOST=
set ANDROID_NDK_PLATFORM=
set ANDROID_NDK_ROOT=
set ANDROID_SDK_ROOT=
set CC=
set CXX=
set INCLUDE=
set CMAKE_INSTALL_PREFIX=
set JAVA_HOME=
set LIB=
set LIBPATH=
set QTDIR=
