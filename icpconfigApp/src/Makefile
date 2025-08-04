TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += icpconfig

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install icpconfig.dbd into <top>/dbd
DBD += icpconfig.dbd

# specify all source files to be compiled and added to the library
icpconfig_SRCS += icpconfig.cpp

icpconfig_LIBS += utilities pugixml
icpconfig_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC += icpconfigCheck icpconfigEnvExpand icpconfigGetMacros icpconfigGetMacro

icpconfigCheck_SRCS += icpconfigCheck.cpp
icpconfigCheck_LIBS += icpconfig utilities libjson pugixml pcre
icpconfigCheck_LIBS += $(EPICS_BASE_IOC_LIBS)

icpconfigEnvExpand_SRCS += icpconfigEnvExpand.cpp
icpconfigEnvExpand_LIBS += icpconfig utilities libjson pugixml pcre
icpconfigEnvExpand_LIBS += $(EPICS_BASE_IOC_LIBS)

icpconfigGetMacros_SRCS += icpconfigGetMacros.cpp
icpconfigGetMacros_LIBS += icpconfig utilities libjson pugixml pcre
icpconfigGetMacros_LIBS += $(EPICS_BASE_IOC_LIBS)

icpconfigGetMacro_SRCS += icpconfigGetMacro.cpp
icpconfigGetMacro_LIBS += icpconfig utilities libjson pugixml pcre
icpconfigGetMacro_LIBS += $(EPICS_BASE_IOC_LIBS)

ifeq ($(SHARED_LIBRARIES),YES)
BIN_INSTALLS_WIN32 += $(PUGIXML)/bin/$(EPICS_HOST_ARCH)/pugixml.dll
BIN_INSTALLS_WIN32 += $(wildcard $(UTILITIES)/bin/$(EPICS_HOST_ARCH)/*.dll)
endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

