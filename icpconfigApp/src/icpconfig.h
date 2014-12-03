#ifndef ICPCONFIG_H
#define ICPCONFIG_H

enum icpOptions { VerboseOutput = 0x1 };
epicsShareExtern int icpconfigCheck(const std::string& configName, const std::string& ioc_name, const std::string& ioc_group, const std::string& configHost, const std::string& configBase, int options);

#endif /* ICPCONFIG_H */
