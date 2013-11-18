# Copyright (c) 2004, 2013, Oracle and/or its affiliates. All rights reserved.
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

noinst_LTLIBRARIES = libportlib.la

libportlib_la_SOURCES = \
	NdbCondition.c NdbMutex.c NdbTick.cpp \
	NdbEnv.c NdbThread.c NdbHost.c NdbTCP.cpp	    \
	ndb_daemon.cc NdbMem.c \
	NdbConfig.c NdbDir.cpp ndb_socket.cpp \
        NdbMutex_DeadlockDetector.cpp \
        NdbGetRUsage.cpp NdbLockCpuUtil.c

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_util.mk.am

INCLUDES += -I$(top_srcdir)/ndb/include/util -I$(top_srcdir)/ndb/include/ndbapi

EXTRA_PROGRAMS = memtest PortLibTest munmaptest

PortLibTest_SOURCES = NdbPortLibTest.cpp
munmaptest_SOURCES = munmaptest.cpp

noinst_PROGRAMS = NdbDir-t NdbNuma-t NdbGetInAddr-t

NdbDir_t_SOURCES = NdbDir.cpp \
       $(top_srcdir)/storage/ndb/src/common/util/basestring_vsnprintf.c
NdbDir_t_CXXFLAGS = -DTEST_NDBDIR
NdbDir_t_LDADD = \
       libportlib.la \
       $(top_builddir)/mysys/libmysyslt.la

NdbNuma_t_SOURCES = NdbNuma.cpp 
NdbNuma_t_CXXFLAGS = -DTEST_NDBNUMA
NdbNuma_t_LDADD = $(top_builddir)/mysys/libmysyslt.la @LIBDL@

NdbGetInAddr_t_SOURCES = NdbTCP.cpp
NdbGetInAddr_t_CXXFLAGS = -DTEST_NDBGETINADDR
