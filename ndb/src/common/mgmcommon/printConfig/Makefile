include .defs.mk

TYPE := ndbapi mgmapiclient

BIN_TARGET := printConfig
BIN_TARGET_ARCHIVES := general portlib

CCFLAGS_LOC += -I.. 

SOURCES := printConfig.cpp

SOURCES.c := ../ConfigRetriever.c ../NdbConfig.c ../LocalConfig.c

include $(NDB_TOP)/Epilogue.mk
