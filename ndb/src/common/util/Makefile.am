
noinst_LTLIBRARIES = libgeneral.la

libgeneral_la_SOURCES = \
	    File.cpp md5_hash.cpp Properties.cpp socket_io.cpp \
            SimpleProperties.cpp Parser.cpp InputStream.cpp \
            SocketServer.cpp SocketClient.cpp SocketAuthenticator.cpp\
	    OutputStream.cpp NdbOut.cpp BaseString.cpp Base64.cpp \
	    NdbSqlUtil.cpp new.cpp \
            uucode.c random.c getarg.c version.c \
            strdup.c strlcat.c strlcpy.c \
            ConfigValues.cpp ndb_init.c

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
