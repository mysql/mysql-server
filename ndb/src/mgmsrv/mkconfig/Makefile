include .defs.mk

TYPE := ndbapi

BIN_TARGET := mkconfig
BIN_TARGET_ARCHIVES := logger general trace mgmsrvcommon portlib

SOURCES := mkconfig.cpp

CCFLAGS_LOC += -I.. -I$(call fixpath,$(NDB_TOP)/src/common/mgmcommon) 

OBJECTS_LOC := ../convertStrToInt.o

include $(NDB_TOP)/Epilogue.mk
