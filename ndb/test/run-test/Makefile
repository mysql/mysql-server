include .defs.mk

TYPE := util 

BIN_TARGET := atrt
BIN_TARGET_LIBS := mgmapi

SOURCES = main.cpp
SCRIPTS = atrt-analyze-result.sh atrt-gather-result.sh atrt-setup.sh \
          atrt-clear-result.sh make-config.sh

OBJECTS_LOC = $(call fixpath,$(NDB_TOP)/src/mgmclient/CpcClient.o)

CFLAGS_main.cpp := -I$(call fixpath,$(NDB_TOP)/src/mgmclient)
CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/include/mgmapi)

include $(NDB_TOP)/Epilogue.mk

_bins::
	-rm -f $(SCRIPTS:%=$(NDB_TOP)/bin/%)
	cp $(SCRIPTS) $(NDB_TOP)/bin

