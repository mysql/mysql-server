include .defs.mk

DIRS := 

ifeq ($(NDB_OS), SOFTOSE)
DIRS += ose
else
ifeq ($(NDB_OS), OSE)
DIRS += ose
else
ifeq ($(NDB_OS), WIN32)
DIRS += win32
else
DIRS += unix
endif
endif
endif


include $(NDB_TOP)/Epilogue.mk

