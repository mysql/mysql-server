include .defs.mk

DIRS := 

ifeq ($(NDB_OS), SOFTOSE)
DIRS += ose
endif

ifeq ($(NDB_OS), OSE)
DIRS += ose
endif

ifeq ($(NDB_OS), SIMCELLO)
DIRS += ose
endif

ifeq ($(NDB_OS), LINUX)
DIRS += unix
endif

ifeq ($(NDB_OS), MACOSX)
DIRS += unix
endif

ifeq ($(NDB_OS), SOLARIS)
DIRS += unix
endif

ifeq ($(NDB_OS), SOLARIS6)
DIRS += unix
endif

ifeq ($(NDB_OS), HPUX)
DIRS += unix
endif

ifeq ($(NDB_OS), WIN32)
DIRS += win32
endif


include $(NDB_TOP)/Epilogue.mk

