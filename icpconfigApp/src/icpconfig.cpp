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
#else
#include <unistd.h>
#endif

#include "pugixml.hpp"

#include "utilities.h"

#include <epicsExport.h>

#include "icpconfig.h"

static bool simulate = false, devsim = false, recsim = false; 

static int icpconfigLoad(int options, const char *iocName, const char* configBase);
static int loadConfig(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose);
static int loadComponent(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose);
static void setValue(MAC_HANDLE *h, const char* name, const char* value, const char* source);
static int loadMacroFile(MAC_HANDLE *h, const std::string& file, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose);

static int readFile(const std::string& filename, std::list<std::string>& lines)
{
    char buffer[128];
    lines.clear();
	std::fstream f;
	f.open(filename.c_str());
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
	return (int)lines.size();
}

struct PVItem
{
    bool defined;
	std::string value;
	std::string source;
	PVItem() : defined(false), value(""), source("unknown") { }
	PVItem(const std::string& value_, const std::string& source_) : defined(true), value(value_), source(source_) { }
};

static std::map<std::string,PVItem> pv_map;

struct PVSetItem
{
    bool defined;
    bool enabled;
	std::string source;
	PVSetItem() : defined(false), enabled(false), source("unknown") { }
	PVSetItem(bool enabled_, const std::string& source_) : defined(true), enabled(enabled_), source(source_) { }
};

static std::map<std::string,PVSetItem> pvset_map;

struct MacroItem
{
    bool defined;
	std::string value;
	std::string source;
	MacroItem() : defined(false), value(""), source("unknown") { }
	MacroItem(const std::string& value_, const std::string& source_) : defined(true), value(value_), source(source_) { }
};

static std::map<std::string,MacroItem> macro_map;

std::list<std::string> load_list;
std::list<std::string> old_load_list;

static std::string readFile(const std::string& filename)
{
	std::list<std::string> lines;
	if (readFile(filename, lines) < 0)
	{
		return "";
	}
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
    const char *ifsim, *ifnotsim, *simsfx;  
    bool is_yes = ( (atoi(value) != 0) || (value[0] == 'y') || (value[0] == 'Y') );
    if ( !strcmp(name, "SIMULATE") )
    {
        if (is_yes)
        {
			ifsim = " ";
			ifnotsim = "#";
//            simsfx = "_sim";
            simsfx = "";
            simulate = true;
        }
        else
        {
			ifsim = "#";
			ifnotsim = " ";
            simsfx = "";
            simulate = false;
        }
		setValue(h, "IFSIM", ifsim, source);
		setValue(h, "IFNOTSIM", ifnotsim, source);
		setValue(h, "SIMSFX", simsfx, source);
    }
    if ( !strcmp(name, "RECSIM") )
    {
        if (is_yes)
        {
			ifsim = " ";
			ifnotsim = "#";
            recsim = true;
        }
        else
        {
			ifsim = "#";
			ifnotsim = " ";
            recsim = false;
        }
		setValue(h, "IFRECSIM", ifsim, source);
		setValue(h, "IFNOTRECSIM", ifnotsim, source);
    }
    if ( !strcmp(name, "DEVSIM") )
    {
        if (is_yes)
        {
			ifsim = " ";
			ifnotsim = "#";
            devsim = true;
        }
        else
        {
			ifsim = "#";
			ifnotsim = " ";
            devsim = false;
        }
		setValue(h, "IFDEVSIM", ifsim, source);
		setValue(h, "IFNOTDEVSIM", ifnotsim, source);
    }
    if ( !strcmp(name, "DISABLE") )
    {
        if (is_yes)
        {
			ifsim = " ";
			ifnotsim = "#";
        }
        else
        {
			ifsim = "#";
			ifnotsim = " ";
        }
		setValue(h, "IFDISABLE", ifsim, source);
		setValue(h, "IFNOTDISABLE", ifnotsim, source);
    }
}

static void setValue(MAC_HANDLE *h, const char* name, const char* value, const char* source)
{
    MacroItem& item = macro_map[name];
    if (item.defined)
    {
	    printf("icpconfigLoad: $(%s)=\"%s\" [previous \"%s\" (%s)]\n", name, value, item.value.c_str(), item.source.c_str());
    }	
	else
	{
	    printf("icpconfigLoad: $(%s)=\"%s\"\n", name, value);
	}
	item = MacroItem(value, source);
	macPutValue(h, name, value);
	epicsEnvSet(name, value);
    checkSpecialVals(h, name, value, source);
}

static void cleanName(std::string& item)
{
    std::transform(item.begin(), item.end(), item.begin(), ::toupper);
	for(size_t i=0; i<item.size(); ++i)
	{
	    if ( !isalnum(item[i]) )
		{
		    item[i] = '_';
		}
	}
}

static void icpconfigReport()
{
	const char* config_root = macEnvExpand("$(ICPCONFIGROOT)");
	printf("icpconfigReport: *** Config Details ***\n");
	printf("icpconfigReport: config root (ICPCONFIGROOT) is \"%s\"\n", (config_root != NULL ? config_root : "(NULL)"));
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
	printf("icpconfigReport:");
    for(std::list<std::string>::const_iterator it = load_list.begin(); it != load_list.end(); ++it)
	{
	    printf(" \"%s\"", it->c_str()); 
    }
	printf("\n");
	if (old_load_list.size() > 0)
	{
	    printf("icpconfigReport: *** Loaded Old Macro Files ***\n");
        for(std::list<std::string>::const_iterator it = old_load_list.begin(); it != old_load_list.end(); ++it)
	    {
	        printf("icpconfigReport: \"%s\"\n", it->c_str()); 
        }
	}
 }

static int icpconfigLoadMain(const std::string& config_name, const std::string& ioc_name, const std::string& ioc_group, int options, const std::string& configHost, const std::string& configBase)
{
	MAC_HANDLE *h = NULL;
	const char *config_base, *config_host;
	if (configBase.size() == 0)
	{
		config_base = macEnvExpand("$(ICPCONFIGBASE)");
	}
	else
	{
		config_base = configBase.c_str();
	}
	if (nullOrZeroLength(config_base))
	{
		errlogPrintf("icpconfigLoad: failed (ICPCONFIGBASE environment variable not set and no configBase parameter specified)\n");
		return -1;
	}
	if (configHost.size() == 0)
	{
		config_host = macEnvExpand("$(ICPCONFIGHOST)");
	}
	else
	{
		config_host = configHost.c_str();
	}
	if (nullOrZeroLength(config_host))
	{
		errlogPrintf("icpconfigLoad: failed (ICPCONFIGHOST environment variable not set)\n");
		return -1;
	}
	if (options == 0)
	{
	    options = atoi(macEnvExpand("$(ICPCONFIGOPTIONS)"));
	}
	bool verbose = (options & VerboseOutput);
	std::string configName;
	printf("icpconfigLoad: ioc \"%s\" group \"%s\" options 0x%x host \"%s\"\n", ioc_name.c_str(), ioc_group.c_str(), options, config_host);
	std::string config_root = macEnvExpand("$(ICPCONFIGROOT)");
	printf("icpconfigLoad: config base (ICPCONFIGBASE) is \"%s\"\n", config_base);
	printf("icpconfigLoad: config root (ICPCONFIGROOT) is \"%s\"\n", config_root.c_str());
    if ( macCreateHandle(&h, NULL) )
	{
		errlogPrintf("icpconfigLoad: failed (macCreateHandle)\n");
	    return -1;
	}
//  macSuppressWarning(h, TRUE);
    setValue(h, "SIMULATE", "0", "{initial default}");
    setValue(h, "DISABLE", "0", "{initial default}");
    setValue(h, "DEVSIM", "0", "{initial default}");
    setValue(h, "RECSIM", "0", "{initial default}");
	if (config_name.size() == 0)
	{
	    configName = readFile(config_root + "/last_config.txt"); 
	}
	else
	{
	    configName = config_name;
	}
    std::string config_dir = config_root + "/configurations/" + configName;
    setValue(h, "ICPCONFIGDIR", config_dir.c_str(), "{initial default}");
	if (configName.size() == 0)
	{
		errlogPrintf("icpconfigLoad: no current config - $(ICPCONFIGROOT)/last_config.txt not found\n");
	}
	else
	{
	    printf("icpconfigLoad: last configuration was \"%s\" (%s)\n", configName.c_str(), config_dir.c_str());
	    loadConfig(h, configName, config_root, ioc_name, ioc_group, false, false, verbose);
	}
// old style files
    loadMacroFile(h, config_root + "/globals.txt", configName, config_root, ioc_name, ioc_group, false, false, verbose);
	loadMacroFile(h, config_root + "/" + ioc_group + ".txt", configName, config_root, ioc_name, ioc_group, false, false, verbose);
	loadMacroFile(h, config_root + "/" + ioc_name + ".txt", configName, config_root, ioc_name, ioc_group, false, false, verbose);
    loadMacroFile(h, config_root + "/../globals.txt", configName, config_root, ioc_name, ioc_group, false, false, verbose);
	loadMacroFile(h, config_root + "/../" + ioc_group + ".txt", configName, config_root, ioc_name, ioc_group, false, false, verbose);
	loadMacroFile(h, config_root + "/../" + ioc_name + ".txt", configName, config_root, ioc_name, ioc_group, false, false, verbose);

    //
    if (devsim || recsim)
    {
        setValue(h, "SIMULATE", "1", "{icpconfig final adjustment}");
    }
	if (simulate)
	{
	    errlogSevPrintf(errlogMajor, "icpconfigLoad: ******** CONFIG HAS REQUESTED SIMULATION MODE ********\n");
	}
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

/// defines ICPCONFIGDIR based on ICPCONFIGBASE, ICPCONFIGHOST, ICPCONFIGROOT and ICPCONFIGOPTIONS
/// also sets SIMULATE, IFSIM, IFNOTSIM
static int icpconfigLoad(int options, const char *iocName, const char* configBase)
{
	std::string ioc_name = setIOCName(iocName);
	if (ioc_name.size() == 0)
	{
		errlogPrintf("icpconfigLoad: failed (IOC environment variable not set and no IOC name specified)\n");
		return -1;
	}
	std::string ioc_group = getIOCGroup();
	icpconfigLoadMain("", ioc_name, ioc_group, options, "", (configBase != NULL ? configBase : ""));
	return 0;
}

epicsShareExtern int icpconfigCheck(const std::string& configName, const std::string& ioc_name, const std::string& ioc_group, const std::string& configHost, const std::string& configBase, int options)
{
	icpconfigLoadMain(configName, ioc_name, ioc_group, options, configHost, configBase);
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
    // ioc sim level
	printf("icpconfigLoad: Loading IOC sim level \"%s\"\n", config_name.c_str());
	pugi::xpath_query ioc_query("/iocs/ioc[@name=$iocname]", &vars);
	pugi::xpath_node_set ioc_node = ioc_query.evaluate_node_set(doc);
    std::string sim_level;
    bool disable = false;
    if (ioc_node.size() > 0)
    {
	    sim_level = ioc_node[0].node().attribute("simlevel").value();
        disable = ioc_node[0].node().attribute("disable").as_bool();
    }
    if (sim_level == "none")
    {
        setValue(h, "DEVSIM", "0", config_name.c_str());
        setValue(h, "RECSIM", "0", config_name.c_str());
    }
    else if (sim_level == "recsim")
    {
        setValue(h, "DEVSIM", "0", config_name.c_str());
        setValue(h, "RECSIM", "1", config_name.c_str());
    }
    else if (sim_level == "devsim")
    {
        setValue(h, "DEVSIM", "1", config_name.c_str());
        setValue(h, "RECSIM", "0", config_name.c_str());
    }
    else
    {
		errlogPrintf("icpconfigLoad: unknown or unspecified sim level \"%s\" - assuming not simulating\n", sim_level.c_str());
        setValue(h, "DEVSIM", "0", config_name.c_str());
        setValue(h, "RECSIM", "0", config_name.c_str());
    }    
    if (disable)
    {
        setValue(h, "DISABLE", "1", config_name.c_str());
    }
    else
    {
        setValue(h, "DISABLE", "0", config_name.c_str());
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
		PVItem& item = pv_map[name]; 
		if (item.defined)
		{
		    printf("icpconfigLoad: %s=\"%s\" [previous \"%s\" (%s)]\n", 
							name.c_str(), value.c_str(), item.value.c_str(), item.source.c_str());
		}
		else
		{
			printf("icpconfigLoad: %s=\"%s\"\n", name.c_str(), value.c_str());
		}
        item = PVItem(value, config_name);
	}
	// ioc pv sets
	printf("icpconfigLoad: Loading IOC PV sets for \"%s\"\n", config_name.c_str());
	pugi::xpath_query pvsets_query("/iocs/ioc[@name=$iocname]/pvsets/pvset", &vars);
	pugi::xpath_node_set ioc_pvsets = pvsets_query.evaluate_node_set(doc);
	std::string files_dir = config_root + config_name + "/files/";
	for (pugi::xpath_node_set::const_iterator it = ioc_pvsets.begin(); it != ioc_pvsets.end(); ++it)
	{
		std::string name = it->node().attribute("name").value();
		bool enabled = it->node().attribute("enabled").as_bool();
		PVSetItem& item = pvset_map[name];
		if (item.defined)
		{
		    printf("icpconfigLoad: \"%s\" is %s [previous %s (%s)]\n", 
					name.c_str(), (enabled ? "ENABLED" : "DISABLED"), (item.enabled ? "ENABLED" : "DISABLED"), config_name.c_str());
		}
		else
		{
		    printf("icpconfigLoad: \"%s\" is %s\n", name.c_str(), (enabled ? "ENABLED" : "DISABLED"));
		}
        item = PVSetItem(enabled, config_name);
		cleanName(name);
		if (enabled)
		{
            setValue(h, (std::string("IFPVSET")+name).c_str(), " ", config_name.c_str());
            setValue(h, (std::string("PVSET")+name).c_str(), (files_dir+name+".cfg").c_str(), config_name.c_str());
		}
		else
		{
            setValue(h, (std::string("IFPVSET")+name).c_str(), "#", config_name.c_str());
            setValue(h, (std::string("PVSET")+name).c_str(), "", config_name.c_str());
		}
	}
	return 0;
}

static int loadFiles(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
    std::list<std::string> files;
	std::string files_dir = config_root + config_name + "/files";
	getFileList(files_dir, files);
	printf("icpconfigLoad: Found %d files for \"%s\"\n", (int)files.size(), config_name.c_str());
	for(std::list<std::string>::iterator it = files.begin(); it != files.end(); ++it)
	{
	    std::string item = *it;
		cleanName(item);
        setValue(h, item.c_str(), (files_dir + "/" + *it).c_str(), config_name.c_str());
        setValue(h, (std::string("IF")+item).c_str(), " ", config_name.c_str());
	}
	return 0;
}

static int loadComponents(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
	pugi::xml_document doc;
	std::string xfile = config_root + config_name + "/components.xml";
	if (access(xfile.c_str(), 0) != 0)
	{
	    printf("icpconfigLoad: no components for \"%s\"\n", config_name.c_str());
		return 0; // no components
	}
	pugi::xml_parse_result result = doc.load_file(xfile.c_str());
	if (!result)
	{
	    std::cerr << "icpconfigLoad: Error in \"" << xfile << "\" " << result.description() << " at offset " << result.offset << std::endl;
		return -1;
	}
	pugi::xpath_node_set components = doc.select_nodes("/components/component");
	components.sort(); // forward document order
	printf("icpconfigLoad: loading %d component(s) for \"%s\"\n", (int)components.size(), config_name.c_str());
	for (pugi::xpath_node_set::const_iterator it = components.begin(); it != components.end(); ++it)
	{
		std::string component = it->node().attribute("name").value();
		loadComponent(h, component, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
	}
	return 0;
}

static int loadComponent(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
	printf("icpconfigLoad: component \"%s\"\n", config_name.c_str());
	load_list.push_back(config_name);
	std::string config_fullname = std::string("/components/") + config_name;
    loadIOCs(h, config_fullname, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
    loadFiles(h, config_fullname, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
	return 0;
}	

static int loadConfig(MAC_HANDLE *h, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
    static int depth = 0;
	if (depth > 20)
	{
		errlogPrintf("icpconfigLoad: failed (recursion depth)\n");
		return -1;
	}
	++depth;
	printf("icpconfigLoad: configuration \"%s\"\n", config_name.c_str());
	load_list.push_back(config_name);
	std::string config_fullname = std::string("/configurations/") + config_name;
	loadComponents(h, config_fullname, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
    loadIOCs(h, config_fullname, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
    loadFiles(h, config_fullname, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
	--depth;
	return 0;
}

static int setPVValuesStatic()
{
	printf("icpconfigLoad: setPVValuesStatic setting %d pvs (pre iocInit)\n", (int)pv_map.size());
    for(std::map<std::string,PVItem>::const_iterator it = pv_map.begin(); it != pv_map.end(); ++it)
	{
	    printf("icpconfigLoad: %s=\"%s\"\n", it->first.c_str(), it->second.value.c_str());
 	    dbpfStatic(it->first.c_str(), it->second.value.c_str());
	}
	return 0;
}

static int setPVValues()
{
	printf("icpconfigLoad: setPVValues setting %d pvs (post iocInit)\n", (int)pv_map.size());
    for(std::map<std::string,PVItem>::const_iterator it = pv_map.begin(); it != pv_map.end(); ++it)
	{
	    printf("icpconfigLoad: %s=\"%s\"\n", it->first.c_str(), it->second.value.c_str());
	    dbpf(it->first.c_str(), it->second.value.c_str());
	}
	return 0;
}

static int loadMacroFile(MAC_HANDLE *h, const std::string& file, const std::string& config_name, const std::string& config_root, const std::string& ioc_name, const std::string& ioc_group, bool warn_if_not_found, bool filter, bool verbose)
{
	char line_buffer[512];
	char** pairs = NULL;
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
	old_load_list.push_back(file);
	std::string prefix_name = ioc_name + "__";
	std::string prefix_group = ioc_group + "__";
	printf("icpconfigLoad: loading old macro file \"%s\"\n", file.c_str());
	unsigned nval = 0, nval_ioc = 0, nval_group = 0, line_number = 0;
	while(input_file.good())
	{
		line_buffer[0] = '\0';
		input_file.getline(line_buffer, sizeof(line_buffer)-1);
		++line_number;
		if (line_buffer[0] == '#')
        {
            continue;
        }
		if (line_buffer[0] == '<')
		{
			// need to add directory name of "file"
			std::string s = trimString(line_buffer + 1);
			size_t pos = file.find_last_of("\\/");
			if (pos != std::string::npos)
			{
				loadMacroFile(h, file.substr(0,pos+1)+trimString(line_buffer + 1), config_name, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
			}
			else
			{
				loadMacroFile(h, trimString(line_buffer + 1), config_name, config_root, ioc_name, ioc_group, warn_if_not_found, filter, verbose);
			}
			continue;
		}
		if (macParseDefns( h, line_buffer, &pairs ) == -1)
		{
			errlogPrintf("icpconfigLoad: failed (macParseDefns) for \"%s\" from \"%s\" line %d\n", line_buffer, file.c_str(), line_number);
			input_file.close();
			return -1;
		}
        // quotes are preserved by macParseDefns() for name="value" so we need to remove them
		for(int i=0; pairs[i] != NULL; i += 2)
        {
            char* strval = pairs[i+1]; 
			if (strval == NULL) // NULL macro value
			{
				continue;
			}
			std::string s(strval);
            if ( s.size() > 1 && s[0] == '"' && s[s.size()-1] == '"' )
            {
                s.erase(s.size()-1, 1);
                s.erase(0, 1);
                strcpy(strval, s.c_str());
            }
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
                setValue(h, pairs[i], pairs[i+1], file.c_str());
//				printf("icpconfigLoad: $(%s)=\"%s\"\n", pairs[i], pairs[i+1]);
			}
			if ( 0 == s.compare(0, prefix_group.size(), prefix_group) )
			{
				++nval;
				++nval_group;
				setValue(h, pairs[i] + prefix_group.size(), pairs[i+1], file.c_str());
//				printf("icpconfigLoad: $(%s)=\"%s\" (group \"%s\")\n", pairs[i] + prefix_group.size(), pairs[i+1], prefix_group.c_str());
			}
			if (0 == s.compare(0, prefix_name.size(), prefix_name))
			{
				++nval;
				++nval_ioc;
				setValue(h, pairs[i] + prefix_name.size(), pairs[i+1], file.c_str());
//				printf("icpconfigLoad: $(%s)=\"%s\" (ioc \"%s\")\n", pairs[i] + prefix_name.size(), pairs[i+1], prefix_name.c_str());
			}
		}
		free(pairs);
	}		
	input_file.close();
	printf("icpconfigLoad: loaded %u macros from old macro file \"%s\" (%d ioc, %d group)\n", nval, file.c_str(), nval_ioc, nval_group);
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
