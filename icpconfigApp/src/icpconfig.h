#ifndef ICPCONFIG_H
#define ICPCONFIG_H

enum icpOptions { ICPOptionsNone = 0x0, VerboseOutput = 0x1, QuietOutput=0x2 };
epicsShareExtern int icpconfigCheck(const std::string& configName, const std::string& ioc_name, const std::string& ioc_group, const std::string& configHost, const std::string& configBase, int options);
epicsShareExtern int icpconfigEnvExpand(const std::string& inFileName, const std::string& outFileName, const std::string& configName, int quiet);
epicsShareExtern void icpconfigGetMacros(const std::string& iocName, const std::string& configName, const std::string& configHost, std::map<std::string, std::string>& names_and_values);

#endif /* ICPCONFIG_H */
