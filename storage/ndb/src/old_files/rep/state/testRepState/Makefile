include .defs.mk

TYPE := kernel

BIN_TARGET := testRequestor
BIN_TARGET_ARCHIVES := portlib general

CCFLAGS_LOC += -I..

SOURCES = testRequestor.cpp \
          ../Requestor.cpp \
          ../RepState.cpp \
          ../Interval.cpp 

include $(NDB_TOP)/Epilogue.mk
