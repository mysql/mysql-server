include .defs.mk

TYPE := ndbapi 

BIN_TARGET := mgmtclient
BIN_TARGET_LIBS := 
BIN_TARGET_ARCHIVES := trace logger mgmapi general mgmsrvcommon portlib repapi

BIN_TARGET_ARCHIVES += editline

DIRS = test_cpcd

# Source files of non-templated classes (.cpp files)
SOURCES = \
	main.cpp \
	CommandInterpreter.cpp \
	CpcClient.cpp

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/include/mgmapi) \
               -I$(call fixpath,$(NDB_TOP)/src/common/mgmcommon) 

include $(NDB_TOP)/Epilogue.mk

_bins_mkconfig : $(NDB_TOP)/bin/$(BIN_TARGET)

