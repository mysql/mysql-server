include .defs.mk

TYPE := ndbapi mgmapiclient

BIN_TARGET := mgmtsrvr
BIN_TARGET_LIBS := 
BIN_TARGET_ARCHIVES := NDB_API mgmsrvcommon mgmapi general

ifneq ($(USE_EDITLINE), N)
BIN_TARGET_ARCHIVES += editline
DIRS := mkconfig
endif
BIN_TARGET_ARCHIVES += general

BIN_FLAGS += $(TERMCAP_LIB)

# Source files of non-templated classes (.cpp files)
SOURCES = \
	MgmtSrvr.cpp \
	MgmtSrvrGeneralSignalHandling.cpp \
	main.cpp \
	Services.cpp \
	convertStrToInt.cpp \
        NodeLogLevel.cpp \
	NodeLogLevelList.cpp \
	SignalQueue.cpp \
	MgmtSrvrConfig.cpp

ifeq ($(findstring OSE, $(NDB_OS)),)
SOURCES += CommandInterpreter.cpp
endif

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/src/ndbapi) \
               -I$(call fixpath,$(NDB_TOP)/src/mgmapi) \
               -I$(call fixpath,$(NDB_TOP)/include/mgmapi) \
               -I$(call fixpath,$(NDB_TOP)/src/common/mgmcommon) 

include $(NDB_TOP)/Epilogue.mk

_bins_mkconfig : $(NDB_TOP)/bin/$(BIN_TARGET)

