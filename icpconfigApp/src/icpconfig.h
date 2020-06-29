#ifndef ICPCONFIG_H
#define ICPCONFIG_H

enum icpOptions { VerboseOutput = 0x1, QuietOutput=0x2 };
epicsShareExtern int icpconfigCheck(const std::string& configName, const std::string& ioc_name, const std::string& ioc_group, const std::string& configHost, const std::string& configBase, int options);
epicsShareExtern int icpconfigEnvExpand(const std::string& inFileName, const std::string& outFileName, const std::string& configName);
epicsShareExtern std::string icpconfigGetMacros(const std::string& configName, const std::string& ioc_name, const std::string& configHost);

#endif /* ICPCONFIG_H */
