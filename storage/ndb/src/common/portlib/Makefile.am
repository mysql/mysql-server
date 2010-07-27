# Copyright (C) 2004, 2006 MySQL AB
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

noinst_LTLIBRARIES = libportlib.la

libportlib_la_SOURCES = \
	NdbCondition.c NdbMutex.c NdbSleep.c NdbTick.c \
	NdbEnv.c NdbThread.c NdbHost.c NdbTCP.cpp	    \
	NdbDaemon.c NdbMem.c \
	NdbConfig.c

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_util.mk.am

EXTRA_PROGRAMS = memtest PortLibTest munmaptest

PortLibTest_SOURCES = NdbPortLibTest.cpp
munmaptest_SOURCES = munmaptest.cpp

WIN_src = win32/NdbCondition.c \
              win32/NdbDaemon.c \
              win32/NdbEnv.c \
              win32/NdbHost.c \
              win32/NdbMem.c \
              win32/NdbMutex.c \
              win32/NdbSleep.c \
              win32/NdbTCP.c \
              win32/NdbThread.c \
              win32/NdbTick.c

windoze-dsp: libportlib.dsp

libportlib.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-lib.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-lib.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(noinst_LTLIBRARIES)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(WIN_src)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LIB $(LDADD)
