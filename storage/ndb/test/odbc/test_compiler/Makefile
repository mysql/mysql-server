include .defs.mk

TYPE =                  odbcdriver

BIN_TARGET =            test_compiler

SOURCES =               test_compiler.cpp

CCFLAGS_LOC += \
                        -I$(NDB_TOP)/src/client/odbc/common \
			-I$(NDB_TOP)/src/client/odbc/dictionary \
			-I$(NDB_TOP)/src/client/odbc/compiler

CCFLAGS_WARNINGS +=     -Wno-unused

LIBS_SPEC += \
                        -lodbccompiler_pic


include $(NDB_TOP)/Epilogue.mk

