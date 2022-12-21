@echo off

set QT_ROOT=C:\Qt\6.3.1
if exist env-custom.cmd call env-custom.cmd

set QT_MSVC=%QT_ROOT%\msvc2019_64
set QT_MINGW=%QT_ROOT%\mingw_64
set QT_ANDROID=%QT_ROOT%\android_armv7
