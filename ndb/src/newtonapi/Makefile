include .defs.mk

TYPE := ndbapiclient

PIC_ARCHIVE    := Y
ARCHIVE_TARGET := newtonapi

A_LIB       := Y
SO_LIB      := Y
PIC_LIB     := Y

LIB_TARGET  := NEWTON_API
LIB_TARGET_ARCHIVES := $(ARCHIVE_TARGET) NDB_API

SOURCES = \
           dba_binding.cpp \
           dba_process.cpp \
           dba_dac.cpp     \
           dba_init.cpp    \
           dba_schema.cpp  \
           dba_bulkread.cpp \
           dba_error.cpp    \
           dba_config.cpp

CCFLAGS_LOC += -I../include -I$(call fixpath,$(NDB_TOP)/include/portlib) -I$(call fixpath,$(NDB_TOP)/include/util) -I$(call fixpath,$(NDB_TOP)/include/newtonapi) -DDEBUG

include $(NDB_TOP)/Epilogue.mk
