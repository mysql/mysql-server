include .defs.mk

TYPE := ndbapitest

BIN_TARGET := newton_perf
BIN_TARGET_LIBS :=
BIN_TARGET_ARCHIVES := NEWTON_API
SOURCES    := perf.cpp

CCFLAGS_LOC := -I$(call fixpath,$(NDB_TOP)/include/util) -I$(call fixpath,$(NDB_TOP)/include/newtonapi) -I$(call fixpath,$(NDB_TOP)/include/portlib)

include $(NDB_TOP)/Epilogue.mk


