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

#include "icpconfig.h"

#include <epicsExport.h>

int main(int argc, char* argv[])
{
    int quiet = 0;
    std::string inFileName, outFileName, configName;
	if (argc > 1)
	{
	    inFileName = argv[1];
	}
	if (argc > 2)
	{
	    outFileName = argv[2];
	}
	if (argc > 3)
	{
	    configName = argv[3];
	}
	if (argc > 4)
	{
	    quiet = (atoi(argv[4]) != 0 ? 1 : 0);
	}
    icpconfigEnvExpand(inFileName, outFileName, configName, quiet);
    return 0;
}
