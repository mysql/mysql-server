include .defs.mk

TYPE := ndbapitest

SOURCES = mainGenerator.c dbGenerator.c

CCFLAGS_LOC := -I../include -I../../include

OBJECTS = \
   mainGenerator.o\
   dbGenerator.o

BIN_TARGET := DbGenerator
BIN_TARGET_ARCHIVES := lmc_User

include $(NDB_TOP)/Epilogue.mk

