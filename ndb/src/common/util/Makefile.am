
noinst_LTLIBRARIES = libgeneral.la

libgeneral_la_SOURCES = \
	    File.cpp md5_hash.cpp Properties.cpp socket_io.cpp \
            SimpleProperties.cpp Parser.cpp InputStream.cpp \
            SocketServer.cpp SocketClient.cpp SocketAuthenticator.cpp\
	    OutputStream.cpp NdbOut.cpp BaseString.cpp Base64.cpp \
	    NdbSqlUtil.cpp new.cpp \
            uucode.c random.c version.c \
            strdup.c \
            ConfigValues.cpp ndb_init.c basestring_vsnprintf.c \
            Bitmask.cpp

EXTRA_PROGRAMS = testBitmask
testBitmask_SOURCES =  testBitmask.cpp
testBitmask_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/ndb/src/libndbclient.la \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/strings/libmystrings.a

testBitmask.cpp : Bitmask.cpp
	rm -f testBitmask.cpp
	@LN_CP_F@ Bitmask.cpp testBitmask.cpp

testBitmask.o:	$(testBitmask_SOURCES)
		$(CXXCOMPILE) -c $(INCLUDES) -D__TEST_BITMASK__ $<


include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
