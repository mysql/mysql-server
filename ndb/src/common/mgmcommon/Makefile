include .defs.mk

TYPE := ndbapi mgmapiclient

PIC_ARCHIVE    := Y
ARCHIVE_TARGET := mgmsrvcommon

DIRS := printConfig

SOURCES = \
	LocalConfig.cpp \
	Config.cpp \
	ConfigInfo.cpp \
	ConfigRetriever.cpp \
	InitConfigFileParser.cpp \
	IPCConfig.cpp

SOURCES.c = NdbConfig.c

include $(NDB_TOP)/Epilogue.mk






