# Experimental: Unit tests with google gtest (installed in /usr/local/...)

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/sql
AM_LDFLAGS  = -static
DEFS        = -DMYSQL_SERVER @DEFS@

# We need to allow implicit templates when compiling gtest.h
# TODO(didrik): substitute this from configure.
MYSQL_EXTRA_CXXFLAGS = -fimplicit-templates

UNIT_TEST_LIBS= $(top_builddir)/mysys/libmysys.la \
		$(top_builddir)/dbug/libdbug.la \
		$(top_builddir)/strings/libmystrings.la 

UNIT_TEST_OBJS= $(top_builddir)/sql/sql_state.o \
		$(top_builddir)/sql/sql_string.o \
		$(top_builddir)/sql/thr_malloc.o

LDADD = $(UNIT_TEST_OBJS)

if HAVE_GTEST
# TODO(didrik): substitute libgtest.la from configure.
LDADD += libgunit.la /usr/local/lib/libgtest.la $(UNIT_TEST_LIBS)

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
			 $(top_builddir)/unittest/mytap/tap.o $(LDADD)
mdl_mytap_t_SOURCES    = mdl_mytap-t.cc

thread_utils_t_SOURCES = thread_utils-t.cc
endif
