include .defs.mk

DIRS := src tools ndbapi run-test

EXTRA_DIRS = newtonapi

ifeq ($(NDB_ARCH), x86_64)
EXTRA_DIRS = 
endif

DIRS += $(EXTRA_DIRS)

ifneq ($(NDB_ODBC),N)
DIRS += odbc
endif

include $(NDB_TOP)/Epilogue.mk

_bins_ndbapi : _libs_src
