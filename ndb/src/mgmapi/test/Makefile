include .defs.mk

TYPE := mgmapiclient util

BIN_TARGET  := testMgmapi 

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/src/common/mgmcommon) 
#-I$(NDB_TOP)/include/util -I$(NDB_TOP)/include/portlib

# Source files of non-templated classes (.C files)
SOURCES = keso.cpp

include $(NDB_TOP)/Epilogue.mk
