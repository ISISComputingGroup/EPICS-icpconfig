#!../../bin/win32-x86/testicpconfig

## You may have to change testicpconfig to something else
## everywhere it appears in this file

< envPaths

cd ${TOP}

## Register all support components
dbLoadDatabase "dbd/testicpconfig.dbd"
testicpconfig_registerRecordDeviceDriver pdbbase

## Run this to trace the stages of iocInit
#traceIocInit

icpconfigLoad 3 ${IOC} "${TOP}/testicpconfigApp/src"

cd ${TOP}/iocBoot/${IOC}

iocInit

## Start any sequence programs
#seq sncExample, "user=faa59Host"
