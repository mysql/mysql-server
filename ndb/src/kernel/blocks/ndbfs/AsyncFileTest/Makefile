include .defs.mk

TYPE := kernel

BIN_TARGET := aftest
BIN_TARGET_ARCHIVES := ndbfs portlib trace signaldataprint

SOURCES = AsyncFileTest.cpp

CFLAGS_AsyncFileTest.cpp = -I../

include $(NDB_TOP)/Epilogue.mk


# run basic tests
run_test :
	$(NDB_TOP)/bin/$(BIN_TARGET)
	$(NDB_TOP)/bin/$(BIN_TARGET) -n 8 -r 8 -l 10 -remove
	$(NDB_TOP)/bin/$(BIN_TARGET) -n 8 -r 8 -l 10 -reverse -remove
	$(NDB_TOP)/bin/$(BIN_TARGET) -n 8 -r 8 -l 10 -s 512 -remove
	$(NDB_TOP)/bin/$(BIN_TARGET) -n 8 -r 4 -l 1000






