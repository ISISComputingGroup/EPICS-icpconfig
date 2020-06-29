@echo off
REM as we setlocal, MACROS will not be defined after we exit bat script
setlocal
REM give IOC name as first argument to get better filtered output
set "GETMACROS=%~dp0bin\%EPICS_HOST_ARCH%\icpconfigGetMacros.exe"
rem for /f "usebackq tokens^=*^ delims^=^ eol^=" %%a in ( `%GETMACROS%` ) do ( set "MACROS=%%a" )
for /f "usebackq tokens=* delims=" %%a in ( `%GETMACROS%` ) do ( set "MACROS=%%a" )
echo Macro JSON is %MACROS%
