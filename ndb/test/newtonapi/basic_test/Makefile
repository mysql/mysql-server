include .defs.mk

TYPE := util

PIC_ARCHIVE := Y
ARCHIVE_TARGET := newtonbasictestcommon

A_LIB       := Y
SO_LIB      := Y
PIC_LIB     := Y

LIB_TARGET  := NEWTON_BASICTEST_COMMON
LIB_TARGET_ARCHIVES := $(ARCHIVE_TARGET) NEWTON_API


SOURCES = common.cpp

DIRS := basic \
        ptr_binding \
        bulk_read

CCFLAGS_LOC := -I$(call fixpath,$(NDB_TOP)/include/util) -I$(call fixpath,$(NDB_TOP)/include/newtonapi)

include $(NDB_TOP)/Epilogue.mk

