include .defs.mk

TYPE := util

BIN_TARGET := ndbsql

#
#  If BIN_TARGET_LIBS include NDB_ODBC then the ODBC lib is
#  linked into the program and the user does not need to 
#  set up any ODBC stuff to make it work.
#
#  If you want to use this program together with some
#  other DBMS (e.g. MySQL or Oracle), then comment the line below.
#
BIN_TARGET_LIBS =	NDB_ODBC 

#BIN_TARGET_ARCHIVES := mgmapi NDB_API

ifneq ($(USE_EDITLINE), N)
BIN_TARGET_ARCHIVES += editline
#DIRS := mkconfig
endif

BIN_FLAGS += $(TERMCAP_LIB)

#ifneq ($(USE_TERMCAP), N)
#LDFLAGS_LOC = -ltermcap
#endif


# Source files of non-templated classes (.cpp files)
SOURCES = \
	ndbsql.cpp 

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/src/ndbapi) \
               -I$(call fixpath,$(NDB_TOP)/include/mgmapi) \
               -I$(call fixpath,$(NDB_TOP)/include/util) \
               -I$(call fixpath,$(NDB_TOP)/src/common/mgmcommon)


include $(NDB_TOP)/Epilogue.mk

_bins_mkconfig : $(NDB_TOP)/bin/$(BIN_TARGET)

