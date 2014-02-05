#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <exception>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <map>
#include <list>
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

#include "utilities.h"

#include <epicsExport.h>

enum icpOptions { VerboseOutput = 0x1, IgnoreHostName = 0x2 };

extern "C" int icpconfigLoad(int options, const char *iocName, const char* configDir);
static int loadConfig(MAC_HANDLE *h, const std::string& config_name, const std::string& config_dir, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose);
static int loadFile(MAC_HANDLE *h, const std::string& file, const std::string& config_name, const std::string& config_dir, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose);

static bool nullOrZeroLength(const char* str)
{
    return (str == NULL) || (str[0] == '\0'); 
}

int icpconfigLoad(int options, const char *iocName, const char* configDir)
{
	MAC_HANDLE *h = NULL;
	if (configDir == NULL)
	{
		configDir = macEnvExpand("$(ICPCONFIGDIR)");
	}
	if (nullOrZeroLength(configDir))
	{
		errlogPrintf("icpconfigLoad failed (ICPCONFIGDIR environment variable not set and no configDir parameter specified)\n");
		return -1;
	}
	const char* configHost = macEnvExpand("$(ICPCONFIGHOST)");
	if (nullOrZeroLength(configHost))
	{
		errlogPrintf("icpconfigLoad failed (ICPCONFIGHOST environment variable not set)\n");
		return -1;
	}
	if (options == 0)
	{
	    options = atoi(macEnvExpand("$(ICPCONFIGOPTIONS)"));
	}
	bool verbose = (options & VerboseOutput);
	
	std::string ioc_name = setIOCName(iocName);
	if (ioc_name.size() == 0)
	{
		errlogPrintf("icpconfigLoad failed (IOC environment variable not set and no IOC name specified)\n");
		return -1;
	}
	std::string ioc_group = getIOCGroup();
	std::string config_host;
	if (!(options & IgnoreHostName))
	{
	    config_host = configHost;
	}
	printf("icpconfigLoad: ioc \"%s\" group \"%s\" options 0x%x host \"%s\"\n", ioc_name.c_str(), ioc_group.c_str(), options, config_host.c_str());
	std::string config_dir = configDir;
	if (config_host.size() > 0)
	{
		config_dir += "/";
		config_dir += config_host;
	}
	printf("icpconfigLoad: dir \"%s\"\n", config_dir.c_str());
    if ( macCreateHandle(&h, NULL) )
	{
		errlogPrintf("icpconfigLoad: failed (macCreateHandle)\n");
	    return -1;
	}
//  macSuppressWarning(h, TRUE);
	loadConfig(h, "", config_dir, ioc_name, ioc_group, false, false, verbose); // globals
	loadFile(h, config_dir + "/config.txt", "", config_dir, ioc_name, ioc_group, true, false, verbose);
	const char* configName = macEnvExpand("$(ICPCONFIGNAME)");
	if (nullOrZeroLength(configName))
	{
		errlogPrintf("icpconfigLoad failed (ICPCONFIGNAME environment variable not set)\n");
		macDeleteHandle(h);
		return -1;
	}
	loadConfig(h, configName, config_dir, ioc_name, ioc_group, false, false, verbose);
	if (verbose)
	{
		macReportMacros(h);
	}
	macDeleteHandle(h);
	return 0;
}	

static int loadConfig(MAC_HANDLE *h, const std::string& config_name, const std::string& config_dir, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
	static int depth = 0;
	static const std::string stars = "**************************************************************************************************************************";
	if (depth > 20)
	{
		errlogPrintf("icpconfigLoad failed (recursion depth)\n");
		return -1;
	}
	++depth;
	printf("icpconfigLoad: %s config \"%s\"\n", stars.substr(0,depth).c_str(), config_name.c_str());
	std::list<std::string> config_file_list;
	std::string config_base = config_dir + "/" + config_name + "/";
//	DIR* dir = opendir(config_base.c_str());
//	if (dir == NULL)
//	{
//		errlogPrintf("icpconfigLoad failed (\"%s\" directory does not exist)\n", config_base.c_str());
//		return -1;
//	}
//	closedir(dir);
	config_file_list.push_back(config_base + "globals.txt");
	config_file_list.push_back(config_base + ioc_group + ".txt");
	config_file_list.push_back(config_base + ioc_name + ".txt");
	for(std::list<std::string>::const_iterator it = config_file_list.begin(); it != config_file_list.end(); ++it)
	{
		loadFile(h, *it, config_name, config_dir, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
	}
	--depth;
	return 0;
}

static int loadFile(MAC_HANDLE *h, const std::string& file, const std::string& config_name, const std::string& config_dir, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
	static int depth = 0;
	static const std::string stars = "**************************************************************************************************************************";
	char line_buffer[512];
	char** pairs = NULL;
	if (depth > 20)
	{
		errlogPrintf("icpconfigLoad failed (recursion depth)\n");
		return -1;
	}
	std::ifstream input_file;
	input_file.open(file.c_str(), std::ios::in);
	if ( !input_file.good() )
	{
		if (warn_if_not_found)
		{
			errlogPrintf("icpconfigLoad: failed (cannot load file \"%s\")\n", file.c_str());
		}
		return -1;
	}
	std::string prefix_name = ioc_name + "__";
	std::string prefix_group = ioc_group + "__";
	++depth;
	printf("icpconfigLoad: %s file \"%s\"\n", stars.substr(0,depth).c_str(), file.c_str());
	unsigned nval = 0, nval_ioc = 0, nval_group = 0, line_number = 0;
	while(input_file.good())
	{
		line_buffer[0] = '\0';
		input_file.getline(line_buffer, sizeof(line_buffer)-1);
		++line_number;
		if (line_buffer[0] == '<')
		{
			// need to add directory name of "file"
			std::string s = trimString(line_buffer + 1);
			size_t pos = file.find_last_of("\\/");
			if (pos != std::string::npos)
			{
				loadFile(h, file.substr(0,pos+1)+trimString(line_buffer + 1), config_name, config_dir, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
			}
			else
			{
				loadFile(h, trimString(line_buffer + 1), config_name, config_dir, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
			}
			continue;
		}
		if (line_buffer[0] == '+')
		{
			loadConfig(h, trimString(line_buffer + 1), config_dir, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
			continue;
		}
		if (macParseDefns( h, line_buffer, &pairs ) == -1)
		{
			errlogPrintf("icpconfigLoad: failed (macParseDefns) for \"%s\" from \"%s\" line %d\n", line_buffer, file.c_str(), line_number);
			input_file.close();
			--depth;
			return -1;
		}
		macInstallMacros( h, pairs );
		for(int i=0; pairs[i] != NULL; i += 2)
		{
			if (pairs[i+1] == NULL) // NULL macro value
			{
				continue;
			}
			std::string s(pairs[i]);
			if (!filter)
			{
				++nval;
				epicsEnvSet(pairs[i], pairs[i+1]);
				printf("icpconfigLoad: $(%s)=\"%s\"\n", pairs[i], pairs[i+1]);
			}
			if ( 0 == s.compare(0, prefix_group.size(), prefix_group) )
			{
				++nval;
				++nval_group;
				macPutValue(h, pairs[i] + prefix_group.size(), pairs[i+1]);
				epicsEnvSet(pairs[i] + prefix_group.size(), pairs[i+1]);
				printf("icpconfigLoad: $(%s)=\"%s\" (group \"%s\")\n", pairs[i] + prefix_group.size(), pairs[i+1], prefix_group.c_str());
			}
			if (0 == s.compare(0, prefix_name.size(), prefix_name))
			{
				++nval;
				++nval_ioc;
				macPutValue(h, pairs[i] + prefix_name.size(), pairs[i+1]);
				epicsEnvSet(pairs[i] + prefix_name.size(), pairs[i+1]);
				printf("icpconfigLoad: $(%s)=\"%s\" (ioc \"%s\")\n", pairs[i] + prefix_name.size(), pairs[i+1], prefix_name.c_str());
			}
		}
		free(pairs);
	}		
	input_file.close();
	printf("icpconfigLoad: %s set %u macros (%d ioc, %d group)\n", stars.substr(0,depth).c_str(), nval, nval_ioc, nval_group);
	--depth;
    return 0;
}

// EPICS iocsh shell commands 

extern "C" {

static const iocshArg initArg0 = { "options", iocshArgInt };			///< The name of the ioc
static const iocshArg initArg1 = { "iocName", iocshArgString };			///< The name of the ioc
static const iocshArg initArg2 = { "configDir", iocshArgString };			///< The name of the configuration file

static const iocshArg * const initArgs[] = { &initArg0, &initArg1, &initArg2 };

static const iocshFuncDef initFuncDef = {"icpconfigLoad", sizeof(initArgs) / sizeof(iocshArg*), initArgs};

static void initCallFunc(const iocshArgBuf *args)
{
    icpconfigLoad(args[0].ival, args[1].sval, args[2].sval);
}

static void icpconfigRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
}

epicsExportRegistrar(icpconfigRegister);

}

