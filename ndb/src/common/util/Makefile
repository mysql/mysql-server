include .defs.mk

TYPE := util

PIC_ARCHIVE    := Y
ARCHIVE_TARGET := general

SOURCES   = File.cpp md5_hash.cpp Properties.cpp socket_io.cpp \
            SimpleProperties.cpp Parser.cpp InputStream.cpp SocketServer.cpp \
	    OutputStream.cpp NdbOut.cpp BaseString.cpp Base64.cpp \
	    NdbSqlUtil.cpp

SOURCES.c = uucode.c random.c getarg.c version.c

ifeq ($(NDB_OS), OSE)
	SOURCES += NdbErrHnd.cpp
endif
ifeq ($(NDB_OS), OSE)
	SOURCES += NdbErrHnd.cpp
endif
ifdef NDB_STRDUP
	SOURCES.c += strdup.c
endif
ifdef NDB_STRLCAT
        SOURCES.c += strlcat.c
endif
ifdef NDB_STRLCPY
        SOURCES.c += strlcpy.c
endif

DIRS := testSimpleProperties

include $(NDB_TOP)/Epilogue.mk

testNdbSqlUtil: NdbSqlUtil.cpp
	$(CC) -o $@ NdbSqlUtil.cpp $(CCFLAGS) -DNDB_SQL_UTIL_TEST -L$(NDB_TOP)/lib -lportlib -lgeneral
