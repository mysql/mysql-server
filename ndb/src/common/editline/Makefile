include .defs.mk

TYPE		:=

ARCHIVE_TARGET	:= editline

CFLAGS		+= -DANSI_ARROWS -DHAVE_TCGETATTR -DSYS_UNIX 

ifeq ($(NDB_OS), WIN32)
SOURCES = editline_win32.c
else
SOURCES		= complete.c editline.c sysunix.c
endif

DIRS		:= test

include $(NDB_TOP)/Epilogue.mk

