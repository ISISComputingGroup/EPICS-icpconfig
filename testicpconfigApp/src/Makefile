TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = testicpconfig
# testicpconfig.dbd will be created and installed
DBD += testicpconfig.dbd

# testicpconfig.dbd will be made up from these files:
testicpconfig_DBD += base.dbd

# Include dbd files from all support applications:
testicpconfig_DBD += icpconfig.dbd

# Add all the support libraries needed by this IOC
testicpconfig_LIBS += icpconfig utilities libjson pugixml pcre

# testicpconfig_registerRecordDeviceDriver.cpp derives from testicpconfig.dbd
testicpconfig_SRCS += testicpconfig_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
testicpconfig_SRCS_DEFAULT += testicpconfigMain.cpp
testicpconfig_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#testicpconfig_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
testicpconfig_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

