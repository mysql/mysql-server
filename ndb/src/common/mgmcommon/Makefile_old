include .defs.mk

TYPE := ndbapi mgmapiclient

PIC_ARCHIVE    := Y
ARCHIVE_TARGET := mgmsrvcommon

# Removed temporary
DIRS := printConfig

SOURCES = \
	LocalConfig.cpp \
	Config.cpp \
	ConfigInfo.cpp \
	ConfigRetriever.cpp \
	InitConfigFileParser.cpp \
	IPCConfig.cpp

SOURCES.c = NdbConfig.c

CFLAGS_IPCConfig.cpp := -I$(call fixpath,$(NDB_TOP)/src/mgmapi)

include $(NDB_TOP)/Epilogue.mk






