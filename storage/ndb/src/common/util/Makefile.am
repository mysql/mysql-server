# Copyright (C) 2004-2005 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

noinst_LTLIBRARIES = libgeneral.la

libgeneral_la_SOURCES = \
	    File.cpp md5_hash.cpp Properties.cpp socket_io.cpp \
            SimpleProperties.cpp Parser.cpp InputStream.cpp \
            SocketServer.cpp SocketClient.cpp SocketAuthenticator.cpp\
	    OutputStream.cpp NdbOut.cpp BaseString.cpp \
	    NdbSqlUtil.cpp new.cpp \
            uucode.c random.c version.c \
            strdup.c \
            ConfigValues.cpp ndb_init.cpp basestring_vsnprintf.c \
            Bitmask.cpp \
	    ndb_rand.c

EXTRA_PROGRAMS = testBitmask
testBitmask_SOURCES =  testBitmask.cpp
testBitmask_LDFLAGS = @ndb_bin_am_ldflags@ \
  $(top_builddir)/storage/ndb/src/libndbclient.la \
         $(top_builddir)/dbug/libdbuglt.la \
         $(top_builddir)/mysys/libmysyslt.la \
         $(top_builddir)/strings/libmystringslt.la

noinst_PROGRAMS = ndb_show_compat
ndb_show_compat_SOURCES = ndb_show_compat.cpp
ndb_show_compat_LDADD = \
	libgeneral.la \
	$(top_builddir)/storage/ndb/src/common/portlib/libportlib.la \
	$(top_builddir)/dbug/libdbuglt.la \
	$(top_builddir)/mysys/libmysyslt.la \
	$(top_builddir)/strings/libmystringslt.la

noinst_PROGRAMS = ndb_show_compat
ndb_show_compat_SOURCES = ndb_show_compat.cc
ndb_show_compat_LDADD = \
	$(builddir)libgeneral.la \
	$(top_builddir)/storage/ndb/src/common/portlib/libportlib.la \
	$(top_builddir)/dbug/libdbug.a \
	$(top_builddir)/mysys/libmysys.a \
	$(top_builddir)/strings/libmystrings.a

testBitmask.cpp : Bitmask.cpp
	rm -f testBitmask.cpp
	@LN_CP_F@ Bitmask.cpp testBitmask.cpp

testBitmask.o:	$(testBitmask_SOURCES)
		$(CXXCOMPILE) -c $(INCLUDES) -D__TEST_BITMASK__ $<


include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_util.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libgeneral.dsp

libgeneral.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(libgeneral_la_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
