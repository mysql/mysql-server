include .defs.mk

TYPE := ndbapi

NONPIC_ARCHIVE := Y
ARCHIVE_TARGET := signal-sender

BIN_TARGET_LIBS     :=          # -lkalle
BIN_TARGET_ARCHIVES := portlib  # $(NDB_TOP)/lib/libkalle.a

# Source files of non-templated classes (.cpp files)
SOURCES = SignalSender.cpp 

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/src/ndbapi)

include $(NDB_TOP)/Epilogue.mk

###
# Backward compatible
