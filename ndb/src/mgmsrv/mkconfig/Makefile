include .defs.mk

TYPE := ndbapi

BIN_TARGET := mkconfig
BIN_TARGET_ARCHIVES := logger trace mgmsrvcommon portlib general 

SOURCES := mkconfig.cpp

CCFLAGS_LOC += -I.. -I$(call fixpath,$(NDB_TOP)/src/common/mgmcommon) 
CFLAGS_mkconfig.cpp := -I$(call fixpath,$(NDB_TOP)/src/mgmapi)

include $(NDB_TOP)/Epilogue.mk
