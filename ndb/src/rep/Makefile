include .defs.mk

#
#  This "kernel" type should be removed (only need types)
#
TYPE := repserver kernel

DIRS := adapters storage state transfer repapi

BIN_TARGET := ndb_rep

BIN_TARGET_LIBS := 
BIN_TARGET_ARCHIVES += 	editline repstorage repadapters reprequestor reptransfer mgmapi NDB_API mgmsrvcommon

SOURCES = \
          RepMain.cpp \
          Requestor.cpp \
          RequestorSubscriptions.cpp \
	  \
          RepComponents.cpp \
          RepCommandInterpreter.cpp \
	  RepApiService.cpp \
	  RepApiInterpreter.cpp \
          SignalQueue.cpp \
          ExtSender.cpp \
	  dbug_hack.cpp

include $(NDB_TOP)/Epilogue.mk
