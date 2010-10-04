# Copyright (C) 2009 Sun Microsystems, Inc.
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/sql $(GTEST_CPPFLAGS)
AM_CXXFLAGS = $(GTEST_CXXFLAGS)
AM_LDFLAGS  = $(GTEST_LDFLAGS)
AM_LIBS     = $(GTEST_LIBS)
DEFS        = -DMYSQL_SERVER @DEFS@
EXTRA_DIST  = CMakeLists.txt FindGTest.cmake

# Allow implicit templates when compiling gtest.h
override CXXFLAGS += @CXXFLAGS_OVERRIDE@

UNIT_TEST_LIBS= $(top_builddir)/mysys/.libs/libmysys.a \
		$(top_builddir)/dbug/.libs/libdbug.a \
		$(top_builddir)/strings/.libs/libmystrings.a

UNIT_TEST_OBJS= $(top_builddir)/sql/sql_state.o \
		$(top_builddir)/sql/sql_string.o \
		$(top_builddir)/sql/thr_malloc.o

LDADD = libgunit.la $(UNIT_TEST_OBJS) $(UNIT_TEST_LIBS) $(AM_LIBS)

if HAVE_GTEST

noinst_HEADERS      = thread_utils.h

noinst_LTLIBRARIES  = libgunit.la

libgunit_la_SOURCES = \
	gunit_test_main.cc \
	tap_event_listener.cc \
	thread_utils.cc

noinst_PROGRAMS = \
	mdl-t$(EXEEXT) \
	mdl_mytap-t$(EXEEXT) \
	sql_list-t$(EXEEXT) \
	thread_utils-t$(EXEEXT)

sql_list_t_LDADD       = $(top_builddir)/sql/sql_list.o $(LDADD)
sql_list_t_SOURCES     = sql_list-t.cc

mdl_t_LDADD            = $(top_builddir)/sql/mdl.o $(LDADD)
mdl_t_SOURCES          = mdl-t.cc

mdl_mytap_t_CPPFLAGS   = -I$(top_srcdir)/unittest/mytap $(AM_CPPFLAGS)
mdl_mytap_t_LDADD      = $(top_builddir)/sql/mdl.o \
			 $(top_builddir)/unittest/mytap/tap.o \
			 $(LDADD)
mdl_mytap_t_SOURCES    = mdl_mytap-t.cc

thread_utils_t_SOURCES = thread_utils-t.cc

endif
