include .defs.mk

TYPE := util

PIC_ARCHIVE    := Y
ARCHIVE_TARGET := mgmapi

A_LIB   := Y
SO_LIB  := Y
PIC_LIB := Y

#DIRS := test

LIB_TARGET := MGM_API
LIB_TARGET_ARCHIVES := $(ARCHIVE_TARGET) general portlib

# Source files of non-templated classes (.C files)
SOURCES = mgmapi.cpp mgmapi_configuration.cpp

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/include/mgmapi) \
               -I$(call fixpath,$(NDB_TOP)/src/common/mgmcommon)

CCFLAGS += -DNO_DEBUG_MESSAGES

# -I$(NDB_TOP)/src/common/mgmcommon

include $(NDB_TOP)/Epilogue.mk
