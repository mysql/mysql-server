include .defs.mk

TYPE := util

PIC_ARCHIVE    := Y
ARCHIVE_TARGET := transporter
DIRS := basictest perftest

# Source files of non-templated classes (.cpp files)
SOURCES = 				\
	Transporter.cpp \
	SendBuffer.cpp   \
	TCP_Transporter.cpp \
	TransporterRegistry.cpp \
        Packer.cpp

DIRS := basictest perftest

CCFLAGS_LOC += -I$(call fixpath,$(NDB_TOP)/include/kernel) \
               -I$(call fixpath,$(NDB_TOP)/include/transporter)


ifeq ($(NDB_SHM), Y)
SOURCES += SHM_Transporter.cpp
ifeq ($(NDB_OS), WIN32)
SOURCES += SHM_Transporter.win32.cpp
else
SOURCES += SHM_Transporter.unix.cpp
endif
endif

ifeq ($(NDB_SCI), Y)
SOURCES += SCI_Transporter.cpp 	
endif

ifneq ($(findstring OSE, $(NDB_OS)),)
	SOURCES += OSE_Transporter.cpp
	SOURCES += OSE_Receiver.cpp
endif



include $(NDB_TOP)/Epilogue.mk
