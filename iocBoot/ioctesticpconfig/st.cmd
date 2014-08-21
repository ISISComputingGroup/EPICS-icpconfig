#!../../bin/win32-x86/testicpconfig

## You may have to change testicpconfig to something else
## everywhere it appears in this file

< envPaths

cd ${TOP}

## Register all support components
dbLoadDatabase "dbd/testicpconfig.dbd"
testicpconfig_registerRecordDeviceDriver pdbbase

## Load record instances
#dbLoadTemplate "db/userHost.substitutions"
#dbLoadRecords "db/dbSubExample.db", "user=faa59Host"

## Set this to see messages from mySub
#var mySubDebug 1

## Run this to trace the stages of iocInit
#traceIocInit

icpconfigLoad 3 ${IOC} "${TOP}/iocBoot/${IOC}/test_config"

cd ${TOP}/iocBoot/${IOC}

iocInit

## Start any sequence programs
#seq sncExample, "user=faa59Host"
