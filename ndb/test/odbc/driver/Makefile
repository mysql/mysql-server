include .defs.mk

TYPE =			*

BIN_TARGET =		testOdbcDriver

SOURCES =		testOdbcDriver.cpp

CCFLAGS_LOC += \
			-I$(NDB_TOP)/include \
			-I$(NDB_TOP)/include/ndbapi \
			-I$(NDB_TOP)/include/portlib \
			-I$(NDB_TOP)/include/util \
			-I$(NDB_TOP)/test/include \
			-I/usr/local/include

CCFLAGS_WARNINGS +=	-Wno-unused -Wno-sign-compare -Wformat

CCFLAGS_TOP +=		-DHAVE_LONG_LONG -DndbODBC

BIN_TARGET_LIBS =	 NDBT NDB_ODBC

ifeq ($(NDB_OS),SOLARIS)
BIN_TARGET_LIBS +=	dmallocthcxx
CCFLAGS_TOP +=		-DDMALLOC
endif

include $(NDB_TOP)/Epilogue.mk

$(BIN_DIR)$(BIN_TARGET): Makefile
