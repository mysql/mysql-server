include .defs.mk

TYPE = util

BIN_TARGET = ndb_cpcc

SOURCES = cpcc.cpp 
OBJECTS_LOC = $(call fixpath,$(NDB_TOP)/src/mgmclient/CpcClient.o)

CFLAGS_cpcc.cpp := -I$(call fixpath,$(NDB_TOP)/src/mgmclient)

include $(NDB_TOP)/Epilogue.mk
