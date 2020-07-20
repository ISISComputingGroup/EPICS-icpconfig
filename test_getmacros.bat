@echo off
REM as we setlocal, MACROS will not be defined after we exit bat script
setlocal
REM give IOC name as first argument to get better filtered output
set "GETMACROS=%~dp0bin\%EPICS_HOST_ARCH%\icpconfigGetMacros.exe"
set "MYIOCNAME=%1"
REM need this funny syntax to be able to set eol correctly - see google
for /f usebackq^ tokens^=*^ delims^=^ eol^= %%a in ( `%GETMACROS% %MYIOCNAME%` ) do ( set "MACROS=%%a" )
echo Macro JSON is %MACROS%
