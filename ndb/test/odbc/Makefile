include .defs.mk

DIRS += driver

ifneq ($(findstring odbc, $(wildcard /usr/lib/libodbc.so)),)
DIRS +=		dm-unixodbc
endif

ifneq ($(findstring $(NDB_OS), SOLARIS),)
DIRS +=		dm-iodbc
endif

include ${NDB_TOP}/Epilogue.mk
