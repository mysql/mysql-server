
noinst_LTLIBRARIES = libgeneral.la

libgeneral_la_SOURCES = \
	    File.cpp md5_hash.cpp Properties.cpp socket_io.cpp \
            SimpleProperties.cpp Parser.cpp InputStream.cpp \
            SocketServer.cpp SocketClient.cpp SocketAuthenticator.cpp\
	    OutputStream.cpp NdbOut.cpp BaseString.cpp \
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

windoze-dsp: libgeneral.dsp

libgeneral.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libgeneral_la_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB $(LDADD)
