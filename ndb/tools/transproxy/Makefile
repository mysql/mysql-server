include .defs.mk

TYPE =

BIN_TARGET =	transproxy

SOURCES =	transproxy.cpp

CCFLAGS_LOC +=\
		-I$(NDB_TOP)/include/kernel \
		-I$(NDB_TOP)/include/mgmcommon \
		-I$(NDB_TOP)/src/common/mgmcommon \
		-I$(NDB_TOP)/src/mgmsrv

LIBS_LOC +=\
		-L$(NDB_TOP)/lib

LIBS_SPEC +=\
		$(NDB_TOP)/src/mgmsrv/InitConfigFileParser.o \
		$(NDB_TOP)/src/mgmsrv/Config.o \
		$(NDB_TOP)/src/mgmsrv/Container.o \
		$(NDB_TOP)/src/mgmsrv/Str.o \
		$(NDB_TOP)/src/mgmsrv/convertStrToInt.o \
		-lNDB_API \
		-leventlogger \
		-llogger \
		-lportlib

include $(NDB_TOP)/Epilogue.mk
