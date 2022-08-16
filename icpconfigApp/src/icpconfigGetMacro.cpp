#include <iostream>
#include <map>
#include <string>

#include "utilities.h"
#include "icpconfig.h"

#include <epicsExport.h>

int main(int argc, char* argv[])
{
    std::string macroName, iocName, configName, configHost;
	if (argc > 1)
	{
	    macroName = argv[1];
	}
	if (argc > 2)
	{
	    iocName = argv[2];
	}
	if (argc > 3)
	{
	    configName = argv[3];
	}
	if (argc > 4)
	{
	    configHost = argv[4];
	}
	
    std::map<std::string,std::string> names_and_values;
    icpconfigGetMacros(iocName, configName, configHost, names_and_values);
	// If the macro does not exist the program will return an empty string.
    std::cout << names_and_values[macroName] << std::endl;
    return 0;
}
