include .defs.mk

TYPE =          ndbapitest

BIN_TARGET =    testGrepVerify
BIN_TARGET_LIBS += bank
SOURCES =       testGrepVerify.cpp 

CFLAGS_testGrepVerify.cpp   += -I$(call fixpath,$(NDB_TOP)/include/kernel)  -I$(call fixpath,$(NDB_TOP)/include/mgmcommon)

include $(NDB_TOP)/Epilogue.mk
