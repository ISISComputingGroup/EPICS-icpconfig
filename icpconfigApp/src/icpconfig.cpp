#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <exception>
#include <stdexcept>
#include <iostream>
#include <map>
#include <string>
#include <time.h>
#include <sstream>
#include <fstream>

#include "epicsStdlib.h"
#include "epicsString.h"
#include "dbDefs.h"
#include "epicsMutex.h"
#include "dbBase.h"
#include "dbStaticLib.h"
#include "dbFldTypes.h"
#include "dbCommon.h"
#include "dbAccessDefs.h"
#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <iocsh.h>
#include "envDefs.h"
#include "macLib.h"
#include "errlog.h"

#include <epicsExport.h>

extern "C" int icpconfigLoad(const char *iocName, const char* configFile);

int icpconfigLoad(const char *iocName, const char* configFile)
{
	MAC_HANDLE *h = NULL;
	char line_buffer[512];
	char** pairs = NULL;
	const char* configDir = macEnvExpand("$(ICPCONFIGDIR)");
	std::ifstream config_file;
	if (configFile == NULL)
	{
		configFile = macEnvExpand("$(ICPCONFIGDIR)/default.txt");
	}
	config_file.open(configFile, std::ios::in);
	if ( !config_file.good() )
	{
		errlogPrintf("icpconfigLoad failed (cannot load file \"%s\")\n", configFile);
		return -1;
	}
	std::string prefix = std::string(iocName) + "_";
	printf("icpconfigLoad: Loading definitions for \"%s\" from \"%s\"\n", iocName, configFile);
	unsigned nval = 0;
    if ( macCreateHandle(&h, NULL) )
	{
		errlogPrintf("icpconfigLoad failed (macCreateHandle)\n");
	    return -1;
	}
//  macSuppressWarning(h, TRUE);
	while(config_file.good())
	{
		config_file.getline(line_buffer, sizeof(line_buffer)-1);
		if (macParseDefns( h, line_buffer, &pairs ) == -1)
		{
			errlogPrintf("icpconfigLoad failed (macParseDefns) for \"%s\"\n", line_buffer);
			macDeleteHandle(h);
			return -1;
		}
//		macInstallMacros( h, pairs );
		for(int i=0; pairs[i] != NULL; i += 2)
		{
			std::string s(pairs[i]);
			if ( 0 == s.find(prefix) )
			{
				++nval;
				epicsEnvSet(pairs[i] + prefix.size(), pairs[i+1]);
				printf("icpconfigLoad: $(%s)=\"%s\"\n", pairs[i] + prefix.size(), pairs[i+1]);
			}
		}
		free(pairs);
	}		
	config_file.close();
	printf("icpconfigLoad: loaded %u definitions\n", nval);
//	macReportMacros(h);  // needs macInstallMacros() above uncommented too
	macDeleteHandle(h);
    return 0;
}

// EPICS iocsh shell commands 

extern "C" {

static const iocshArg initArg0 = { "iocName", iocshArgString };			///< The name of the ioc
static const iocshArg initArg1 = { "configFile", iocshArgString };			///< The name of the configuration file

static const iocshArg * const initArgs[] = { &initArg0, &initArg1 };

static const iocshFuncDef initFuncDef = {"icpconfigLoad", sizeof(initArgs) / sizeof(iocshArg*), initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    icpconfigLoad(args[0].sval, args[1].sval);
}

static void icpconfigRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(icpconfigRegister);

}

