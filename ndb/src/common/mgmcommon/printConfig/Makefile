include .defs.mk

TYPE := ndbapi mgmapiclient

BIN_TARGET := printConfig
BIN_TARGET_ARCHIVES := general portlib

CCFLAGS_LOC += -I.. 

SOURCES := printConfig.cpp ../ConfigRetriever.cpp

SOURCES.c := ../NdbConfig.c ../LocalConfig.c

CFLAGS_printConfig.cpp := -I$(call fixpath,$(NDB_TOP)/src/mgmapi)

include $(NDB_TOP)/Epilogue.mk
