include .defs.mk

TYPE =			*

BIN_TARGET =		testOdbcDMu

SOURCES =		testOdbcDMu.cpp

CCFLAGS_LOC += \
			-I$(NDB_TOP)/include \
			-I$(NDB_TOP)/include/ndbapi \
			-I$(NDB_TOP)/include/portlib \
			-I$(NDB_TOP)/include/util \
			-I$(NDB_TOP)/test/include

CCFLAGS_WARNINGS +=	-Wno-unused -Wno-sign-compare

CCFLAGS_TOP +=		-DHAVE_LONG_LONG -DunixODBC

BIN_TARGET_LIBS =	NDBT general portlib

ifeq ($(NDB_OS),SOLARIS)
CCFLAGS_TOP +=		-DDMALLOC
LIBS_LOC +=		-L/usr/local/lib
BIN_TARGET_LIBS +=	odbc odbcinst
BIN_TARGET_LIBS +=	dmallocthcxx
endif

ifeq ($(NDB_OS),LINUX)
BIN_TARGET_LIBS +=	odbc odbcinst
BIN_TARGET_LIBS_DIRS +=	.
dummy := $(shell [ ! -f /usr/lib/libodbc.so ] || ln -sf /usr/lib/libodbc.so.1 libodbc.so)
dummy := $(shell [ ! -f /usr/lib/libodbcinst.so ] || ln -sf /usr/lib/libodbcinst.so.1 libodbcinst.so)
endif

include $(NDB_TOP)/Epilogue.mk

testOdbcDMu.cpp:
	ln -s ../driver/testOdbcDriver.cpp $@
