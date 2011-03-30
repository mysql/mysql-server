include .defs.mk

TYPE =			*

BIN_TARGET =		testOdbcDMi

SOURCES =		testOdbcDMi.cpp

CCFLAGS_LOC += \
			-I$(NDB_TOP)/include \
			-I$(NDB_TOP)/include/ndbapi \
			-I$(NDB_TOP)/include/portlib \
			-I$(NDB_TOP)/include/util \
			-I$(NDB_TOP)/test/include

CCFLAGS_WARNINGS +=	-Wno-unused -Wno-sign-compare

CCFLAGS_TOP +=		-DHAVE_LONG_LONG -DiODBC

BIN_TARGET_LIBS =	NDBT general portlib

ifeq ($(NDB_OS),SOLARIS)
CCFLAGS_TOP +=		-DDMALLOC
LIBS_LOC +=		-L/usr/local/opt/iODBC/lib
LIBS_LOC +=		-R/usr/local/opt/iODBC/lib
BIN_TARGET_LIBS +=	iodbc
BIN_TARGET_LIBS +=	dmallocthcxx
endif

ifeq ($(NDB_OS),LINUX)
BIN_TARGET_LIBS_DIRS +=	/usr/local/opt/iODBC/lib
BIN_TARGET_LIBS +=	iodbc
endif

include $(NDB_TOP)/Epilogue.mk

testOdbcDMi.cpp:
	ln -s ../driver/testOdbcDriver.cpp $@
