# Copyright (C) 2004-2005 MySQL AB
#  All rights reserved. Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

EXTRA_DIST = CMakeLists.txt

noinst_LTLIBRARIES = libgeneral.la libndbazio.la

libgeneral_la_SOURCES = \
	    File.cpp md5_hash.cpp Properties.cpp socket_io.cpp \
            SimpleProperties.cpp Parser.cpp InputStream.cpp \
            SocketServer.cpp SocketClient.cpp SocketAuthenticator.cpp\
	    OutputStream.cpp NdbOut.cpp BaseString.cpp \
            NdbSqlUtil.cpp \
            uucode.c random.c version.cpp ndb_opts.c \
            strdup.c \
            ConfigValues.cpp ndb_init.cpp basestring_vsnprintf.c \
            Bitmask.cpp SparseBitmask.cpp parse_mask.hpp \
	    ndb_rand.c require.c Vector.cpp

INCLUDES_LOC = @ZLIB_INCLUDES@

libndbazio_la_SOURCES = azio.c
libndbazio_la_LIBADD = @ZLIB_LIBS@

noinst_PROGRAMS = BaseString-t HashMap-t Bitmask-t SparseBitmask-t ndb_version-t

BaseString_t_SOURCES = BaseString.cpp
BaseString_t_CXXFLAGS = -DTEST_BASE_STRING
BaseString_t_LDADD = \
	libgeneral.la \
	$(top_builddir)/mysys/libmysyslt.la

HashMap_t_SOURCES = HashMap.cpp
HashMap_t_CXXFLAGS = -DTEST_HASH
HashMap_t_LDADD = \
	libgeneral.la \
	$(top_builddir)/mysys/libmysyslt.la \
	$(top_builddir)/dbug/libdbuglt.la \
	$(top_builddir)/strings/libmystringslt.la

Bitmask_t_SOURCES = Bitmask.cpp
Bitmask_t_CXXFLAGS = -DTEST_BITMASK
Bitmask_t_LDADD = \
	libgeneral.la \
	$(top_builddir)/mysys/libmysyslt.la

#Bitmask_p_SOURCES = Bitmask.cpp
#Bitmask_p_CXXFLAGS = -DBENCH_BITMASK
#Bitmask_p_LDADD = \
#	libgeneral.la \
#        ../portlib/libportlib.la \
#	$(top_builddir)/mysys/libmysyslt.la

SparseBitmask_t_SOURCES = SparseBitmask.cpp
SparseBitmask_t_CXXFLAGS = -DTEST_SPARSEBITMASK
SparseBitmask_t_LDADD = \
	libgeneral.la \
	$(top_builddir)/mysys/libmysyslt.la

ndb_version_t_SOURCES = version.cpp
ndb_version_t_CXXFLAGS = -DTEST_VERSION
ndb_version_t_LDADD = libgeneral.la

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_util.mk.am
