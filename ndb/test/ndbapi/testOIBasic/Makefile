include .defs.mk

TYPE =			ndbapitest

BIN_TARGET =		testOIBasic

SOURCES =		testOIBasic.cpp

ifeq ($(NDB_COMPILER),GCC)
CCFLAGS_WARNINGS +=	-Wno-unused -Wformat
endif

include $(NDB_TOP)/Epilogue.mk
