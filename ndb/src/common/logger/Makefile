include .defs.mk

TYPE := ndbapi

PIC_ARCHIVE    := Y
ARCHIVE_TARGET := logger

DIRS := loggertest

SOURCES := Logger.cpp LogHandlerList.cpp LogHandler.cpp \
           ConsoleLogHandler.cpp FileLogHandler.cpp

ifeq ($(NDB_OS), OSE) 
NO_SYSLOG := Y
endif

ifeq ($(NDB_OS), WIN32) 
NO_SYSLOG := Y
endif

ifneq ($(NO_SYSLOG), Y)
SOURCES += SysLogHandler.cpp
endif

include $(NDB_TOP)/Epilogue.mk


