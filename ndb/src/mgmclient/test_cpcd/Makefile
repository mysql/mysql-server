include .defs.mk

TYPE := ndbapi 

BIN_TARGET := test_cpcd
BIN_TARGET_LIBS := general


# Source files of non-templated classes (.cpp files)
SOURCES = test_cpcd.cpp ../CpcClient.cpp

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/include/mgmapi) \
               -I$(call fixpath,$(NDB_TOP)/src/common/mgmcommon) 

include $(NDB_TOP)/Epilogue.mk


