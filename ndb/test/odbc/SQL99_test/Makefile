include .defs.mk

TYPE =                  odbcdriver

BIN_TARGET =            SQL99_test

SOURCES =               SQL99_test.cpp

CCFLAGS_LOC += 			-I/usr/local/include \
				        -I$(NDB_TOP)/test/include \
						-I$(NDB_TOP)/include \
                        -I$(NDB_TOP)/src/client/odbc/common


CCFLAGS_WARNINGS +=     -Wno-unused

LIBS_SPEC += \
						-lNDBT \
						-lodbc \
						-lodbcinst \
						-lportlib



include $(NDB_TOP)/Epilogue.mk

