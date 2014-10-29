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
#include <initHooks.h>
#include "envDefs.h"
#include "macLib.h"
#include "errlog.h"
#include "dbAccess.h"
#include "dbTest.h"
#include "dbStaticLib.h"
#include "errlog.h"

#ifdef _WIN32
#include <io.h>
#endif

#include "pugixml.hpp"

#include "utilities.h"

#include <epicsExport.h>

enum icpOptions { VerboseOutput = 0x1, IgnoreHostName = 0x2 };

static int icpconfigLoad(int options, const char *iocName, const char* configBase);
static int loadConfig(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose);
static int loadComponent(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose);
static void setValue(MAC_HANDLE *h, const char* name, const char* value, const char* source);

static int readFile(const std::string& filename, std::list<std::string>& lines)
{
    char buffer[128];
    lines.clear();
	std::fstream f;
	f.open(filename);
	if ( !f.good() )
	{
		errlogPrintf("icpconfigLoad: file \"%s\" not found\n", filename.c_str());
		return -1;
	}
	for(bool done = false; !done; )
	{
		f.getline(buffer, sizeof(buffer));
		if (f.good())
		{
			lines.push_back(buffer);
		}
		else
		{
		    done = true;
		}
	}
	return lines.size();
}

struct PVItem
{
	std::string value;
	std::string source;
	PVItem() : value(""), source("unknown") { }
	PVItem(const std::string& value_, const std::string& source_) : value(value_), source(source_) { }
};

static std::map<std::string,PVItem> pv_map;

struct PVSetItem
{
    bool enabled;
	std::string source;
	PVSetItem() : enabled(false), source("unknown") { }
	PVSetItem(bool enabled_, const std::string& source_) : enabled(enabled_), source(source_) { }
};

static std::map<std::string,PVSetItem> pvset_map;

struct MacroItem
{
	std::string value;
	std::string source;
	MacroItem() : value(""), source("unknown") { }
	MacroItem(const std::string& value_, const std::string& source_) : value(value_), source(source_) { }
};

static std::map<std::string,MacroItem> macro_map;

std::list<std::string> load_list;

static std::string readFile(const std::string& filename)
{
	std::list<std::string> lines;
	readFile(filename, lines);
	if (lines.size() > 0)
	{
	    return lines.front();
	}
	else
	{
	    return "";
	}
}

// a version of dbpf() that can be used prior to iocInit (i.e. static database access)
static long dbpfStatic(const char   *pname, const char *pvalue) 
{
	DBENTRY         dbentry;  
	int             status;	
	if (!pdbbase) 
	{
		errlogPrintf("dbpfStatic: No database loaded\n");
		return -1;
	}
	dbInitEntry(pdbbase, &dbentry);	 
	if ( (status = dbFindRecord(&dbentry, pname)) == 0 ) 
	{
		status = dbPutString(&dbentry, pvalue);
		if ( status != 0 )
		{
			errlogPrintf("dbpfStatic: error setting \"%s\"=\"%s\"\n", pname, pvalue);
		}
	}
	else
	{
		errlogPrintf("dbpfStatic: cannot find \"%s\"\n", pname);
	}
	dbFinishEntry(&dbentry);
	return status;
}

static bool nullOrZeroLength(const char* str)
{
    return (str == NULL) || (str[0] == '\0'); 
}

/// define IFSIM and IFNOTSIM depending on value of SIMULATE
static void checkSpecialVals(MAC_HANDLE *h, const char* name, const char* value, const char* source)
{
    if ( !strcmp(name, "SIMULATE") )
    {
        const char *ifsim, *ifnotsim, *simsfx;  
        if ( (atoi(value) != 0) || (value[0] == 'y') || (value[0] == 'Y') )
        {
			ifsim = " ";
			ifnotsim = "#";
            simsfx = "_sim";
        }
        else
        {
			ifsim = "#";
			ifnotsim = " ";
            simsfx = "";
        }
		setValue(h, "IFSIM", ifsim, source);
		setValue(h, "IFNOTSIM", ifnotsim, source);
		setValue(h, "SIMSFX", simsfx, source);
    }
}

static void setValue(MAC_HANDLE *h, const char* name, const char* value, const char* source)
{
	printf("icpconfigLoad: $(%s)=\"%s\"\n", name, value);
	macro_map[name] = MacroItem(value, source);
	macPutValue(h, name, value);
	epicsEnvSet(name, value);
    checkSpecialVals(h, name, value, source);
}

static void icpconfigReport()
{
	printf("icpconfigReport: *** Macro report ***\n");
	for(std::map<std::string,MacroItem>::const_iterator it = macro_map.begin(); it != macro_map.end(); ++it)
	{
		printf("icpconfigReport: $(%s)=\"%s\" (%s)\n", it->first.c_str(), it->second.value.c_str(), it->second.source.c_str());
	}
	printf("icpconfigReport: *** PV Report ***\n");
    for(std::map<std::string,PVItem>::const_iterator it = pv_map.begin(); it != pv_map.end(); ++it)
	{
		printf("icpconfigReport: %s=\"%s\" (%s)\n", it->first.c_str(), it->second.value.c_str(), it->second.source.c_str());
	}
	printf("icpconfigReport: *** PVSet Report ***\n");
    for(std::map<std::string,PVSetItem>::const_iterator it = pvset_map.begin(); it != pvset_map.end(); ++it)
	{
		printf("icpconfigReport: \"%s\" is ENABLED (%s)\n", it->first.c_str(), it->second.source.c_str());
	}
	printf("icpconfigReport: *** Loaded Configurations and Components ***\n");
	printf("icpconfigReport: ");
    for(std::list<std::string>::const_iterator it = load_list.begin(); it != load_list.end(); ++it)
	{
	    printf(" \"%s\"", it->c_str()); 
    }
	printf("\n");
 }

/// defines ICPCONFIGROOT and ICPCONFIGDIR based on ICPCONFIGBASE, ICPCONFIGHOST and ICPCONFIGOPTIONS
/// also sets SIMULATE, IFSIM, IFNOTSIM
static int icpconfigLoad(int options, const char *iocName, const char* configBase)
{
	MAC_HANDLE *h = NULL;
	std::string ioc_name = setIOCName(iocName);
	if (ioc_name.size() == 0)
	{
		errlogPrintf("icpconfigLoad: failed (IOC environment variable not set and no IOC name specified)\n");
		return -1;
	}
	std::string ioc_group = getIOCGroup();
	if (configBase == NULL)
	{
		configBase = macEnvExpand("$(ICPCONFIGBASE)");
	}
	if (nullOrZeroLength(configBase))
	{
		errlogPrintf("icpconfigLoad: failed (ICPCONFIGBASE environment variable not set and no configBase parameter specified)\n");
		return -1;
	}
	const char* configHost = macEnvExpand("$(ICPCONFIGHOST)");
	if (nullOrZeroLength(configHost))
	{
		errlogPrintf("icpconfigLoad: failed (ICPCONFIGHOST environment variable not set)\n");
		return -1;
	}
	if (options == 0)
	{
	    options = atoi(macEnvExpand("$(ICPCONFIGOPTIONS)"));
	}
	bool verbose = (options & VerboseOutput);
	std::string config_host;
	if (!(options & IgnoreHostName))
	{
	    config_host = configHost;
	}
	printf("icpconfigLoad: ioc \"%s\" group \"%s\" options 0x%x host \"%s\"\n", ioc_name.c_str(), ioc_group.c_str(), options, config_host.c_str());
	std::string config_root = configBase;
	if (config_host.size() > 0)
	{
		config_root += "/";
		config_root += config_host;
	}
	config_root += "/configurations";
	printf("icpconfigLoad: config root (ICPCONFIGROOT) set to \"%s\"\n", config_root.c_str());
    if ( macCreateHandle(&h, NULL) )
	{
		errlogPrintf("icpconfigLoad: failed (macCreateHandle)\n");
	    return -1;
	}
//  macSuppressWarning(h, TRUE);
    setValue(h, "ICPCONFIGROOT", config_root.c_str(), "");
	std::string configName = readFile(config_root + "/last_config.txt");
	if (configName.size() == 0)
	{
		errlogPrintf("icpconfigLoad: no current config - $(ICPCONFIGROOT)/last_config.txt not found\n");
	    return -1;
	}
	printf("icpconfigLoad: last configuration was \"%s\"\n", configName.c_str());
    setValue(h, "SIMULATE", "0", "");
    std::string config_dir = config_root + configName;
    setValue(h, "ICPCONFIGDIR", config_dir.c_str(), "");
	loadConfig(h, configName, config_root, ioc_name, ioc_group, false, false, verbose);
//	if (verbose)
//	{
//		printf("*** Macro report ***\n");
//		macReportMacros(h);
//	}
	macDeleteHandle(h);
	if (verbose)
	{
	    icpconfigReport();
	}
	return 0;
}	


static int loadIOCs(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
	pugi::xml_document doc;
	std::string xfile = config_root + config_name + "/iocs.xml";
	pugi::xml_parse_result result = doc.load_file(xfile.c_str());
	if (!result)
	{
	    std::cerr << "icpconfigLoad: Error in \"" << xfile << "\" " << result.description() << " at offset " << result.offset << std::endl;
		return -1;
	}
	pugi::xpath_variable_set vars;
	vars.add("iocname", pugi::xpath_type_string);
	vars.add("iocgroup", pugi::xpath_type_string);
	vars.set("iocname",ioc_name.c_str());
	vars.set("iocgroup",ioc_group.c_str());
	// default macros
	printf("icpconfigLoad: Loading default macros for \"%s\"\n", config_name.c_str());
	pugi::xpath_node_set default_macros = doc.select_nodes("/iocs/defaults/macros/macro");
	default_macros.sort(); // forward document order
	for (pugi::xpath_node_set::const_iterator it = default_macros.begin(); it != default_macros.end(); ++it)
	{
		std::string name = it->node().attribute("name").value();
		std::string value = it->node().attribute("value").value();
        setValue(h, name.c_str(), value.c_str(), config_name.c_str());
	}
	// ioc macros
	printf("icpconfigLoad: Loading IOC macros for \"%s\"\n", config_name.c_str());
	pugi::xpath_query macros_query("/iocs/ioc[@name=$iocname]/macros/macro", &vars);
	pugi::xpath_node_set ioc_macros = macros_query.evaluate_node_set(doc);
	for (pugi::xpath_node_set::const_iterator it = ioc_macros.begin(); it != ioc_macros.end(); ++it)
	{
		std::string name = it->node().attribute("name").value();
		std::string value = it->node().attribute("value").value();
        setValue(h, name.c_str(), value.c_str(), config_name.c_str());
	}
	// ioc pvs
	printf("icpconfigLoad: Loading IOC PVs for \"%s\"\n", config_name.c_str());
	pugi::xpath_query pvs_query("/iocs/ioc[@name=$iocname]/pvs/pv", &vars);
	pugi::xpath_node_set ioc_pvs = pvs_query.evaluate_node_set(doc);
	for (pugi::xpath_node_set::const_iterator it = ioc_pvs.begin(); it != ioc_pvs.end(); ++it)
	{
		std::string name = it->node().attribute("name").value();
		std::string value = it->node().attribute("value").value();
        pv_map[name] = PVItem(value, config_name);
		printf("icpconfigLoad: PV %s=\"%s\"\n", name.c_str(), value.c_str());
	}
	// ioc pv sets
	printf("icpconfigLoad: Loading IOC PV sets for \"%s\"\n", config_name.c_str());
	pugi::xpath_query pvsets_query("/iocs/ioc[@name=$iocname]/pvsets/pvset", &vars);
	pugi::xpath_node_set ioc_pvsets = pvsets_query.evaluate_node_set(doc);
	for (pugi::xpath_node_set::const_iterator it = ioc_pvsets.begin(); it != ioc_pvsets.end(); ++it)
	{
		std::string name = it->node().attribute("name").value();
		bool enabled = it->node().attribute("enabled").as_bool();
		if (enabled)
		{
            pvset_map[name] = PVSetItem(enabled, config_name);
		    printf("icpconfigLoad: PVSET \"%s\" is ENABLED\n", name.c_str());
		}			
	}
	return 0;
}

static int loadFiles(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
    std::list<std::string> files;
	std::string files_dir = config_root + config_name + "/files";
	getFileList(files_dir, files);
	printf("icpconfigLoad: Found %d files for \"%s\"\n", files.size(), config_name.c_str());
	for(std::list<std::string>::iterator it = files.begin(); it != files.end(); ++it)
	{
	    std::string item = *it;
        std::transform(item.begin(), item.end(), item.begin(), ::toupper);
		for(int i=0; i<item.size(); ++i)
		{
		    if ( !isalnum(item[i]) )
			{
			    item[i] = '_';
			}
		}
        setValue(h, item.c_str(), (files_dir + "/" + *it).c_str(), config_name.c_str());
        setValue(h, (std::string("IF")+item).c_str(), " ", config_name.c_str());
	}
	return 0;
}

static int loadSubconfigs(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
	pugi::xml_document doc;
	std::string xfile = config_root + config_name + "/subconfigs.xml";
	if (access(xfile.c_str(), 0) != 0)
	{
	    printf("icpconfigLoad: no subconfigs for \"%s\"\n", config_name.c_str());
		return 0; // no subconfigs
	}
	pugi::xml_parse_result result = doc.load_file(xfile.c_str());
	if (!result)
	{
	    std::cerr << "icpconfigLoad: Error in \"" << xfile << "\" " << result.description() << " at offset " << result.offset << std::endl;
		return -1;
	}
	pugi::xpath_node_set subconfigs = doc.select_nodes("/subconfigs/subconfig");
	subconfigs.sort(); // forward document order
	printf("icpconfigLoad: loading %d subconfigs for \"%s\"\n", subconfigs.size(), config_name.c_str());
	for (pugi::xpath_node_set::const_iterator it = subconfigs.begin(); it != subconfigs.end(); ++it)
	{
		std::string subConfig = it->node().attribute("name").value();
		loadComponent(h, subConfig, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
	}
	return 0;
}

static int loadComponent(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
    static int depth = 0;
	if (depth > 20)
	{
		errlogPrintf("icpconfigLoad: failed (recursion depth)\n");
		return -1;
	}
	++depth;
	printf("icpconfigLoad: component \"%s\"\n", config_name.c_str());
	load_list.push_back(config_name);
	loadSubconfigs(h, config_name, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
    loadIOCs(h, config_name, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
    loadFiles(h, config_name, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
	--depth;
	return 0;
}	

static int loadConfig(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
    return loadComponent(h, config_name, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
}

static int setPVValuesStatic()
{
	printf("icpconfigLoad: setPVValuesStatic setting %d pvs (pre iocInit)\n", pv_map.size());
    for(std::map<std::string,PVItem>::const_iterator it = pv_map.begin(); it != pv_map.end(); ++it)
	{
	    dbpfStatic(it->first.c_str(), it->second.value.c_str());
	}
	return 0;
}

static int setPVValues()
{
	printf("icpconfigLoad: setPVValues setting %d pvs (post iocInit)\n", pv_map.size());
    for(std::map<std::string,PVItem>::const_iterator it = pv_map.begin(); it != pv_map.end(); ++it)
	{
	    dbpf(it->first.c_str(), it->second.value.c_str());
	}
	return 0;
}


// EPICS iocsh shell commands 

static const iocshArg initArg0 = { "options", iocshArgInt };			///< options
static const iocshArg initArg1 = { "iocName", iocshArgString };			///< The name of the ioc
static const iocshArg initArg2 = { "configBase", iocshArgString };			///< The name of the configuration file

static const iocshArg * const initArgs[] = { &initArg0, &initArg1, &initArg2 };

static const iocshFuncDef initFuncDef = {"icpconfigLoad", sizeof(initArgs) / sizeof(iocshArg*), initArgs};


static void initCallFunc(const iocshArgBuf *args)
{
    icpconfigLoad(args[0].ival, args[1].sval, args[2].sval);
}

static const iocshFuncDef repFuncDef = {"icpconfigReport", 0, NULL};

static void repCallFunc(const iocshArgBuf *args)
{
    icpconfigReport();
}

extern "C" {

static void icpconfigRegister(void)
{
    iocshRegister(&initFuncDef, initCallFunc);
	iocshRegister(&repFuncDef, repCallFunc);
}

epicsExportRegistrar(icpconfigRegister);

}

/*
 * INITHOOKS
 *
 * called by iocInit at various points during initialization
 *
 */

/* If this function (initHooks) is loaded, iocInit calls this function
 * at certain defined points during IOC initialization */
static void icpInitHooks(initHookState state)
{
	switch (state) {
	case initHookAtIocBuild :
		break;
	case initHookAtBeginning :
	    break;
	case initHookAfterCallbackInit :
	    break;
	case initHookAfterCaLinkInit :
	    break;
	case initHookAfterInitDrvSup :
	    break;
	case initHookAfterInitRecSup :
	    break;
	case initHookAfterInitDevSup : // Autosave pass 0 uses this
	    break;
	case initHookAfterInitDatabase : // Autosave pass 1 uses this
	    break;
	case initHookAfterFinishDevSup :
		setPVValuesStatic();      // do it here so we are sure to overwrite autosave values before PINI 
	    break;
	case initHookAfterScanInit :
	    break;
	case initHookAfterInitialProcess : // PINI processing happens just before this
	    break;
	case initHookAfterCaServerInit :
	    break;
	case initHookAfterIocBuilt :
	    break;
    case initHookAtIocRun :
	    break;
    case initHookAfterDatabaseRunning :
	    break;
    case initHookAfterCaServerRunning :
	    break;
	case initHookAfterIocRunning :
		setPVValues();    // this will probably trigger record processing of PVs concerned 
	    break;
	default:
	    break;
	}
	return;
}

extern "C" {

static void icpInitHooksRegister(void)
{
   initHookRegister(icpInitHooks);
}

epicsExportRegistrar(icpInitHooksRegister);

}
