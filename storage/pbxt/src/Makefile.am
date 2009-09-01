# Used to build Makefile.in

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =          $(pkglibdir)/plugin
INCLUDES=		-I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/storage/innobase/include \
			-I$(top_srcdir)/sql \
			-I$(srcdir)

LIBS =

LDADD =

noinst_HEADERS =		bsearch_xt.h cache_xt.h ccutils_xt.h database_xt.h \
						datadic_xt.h datalog_xt.h filesys_xt.h hashtab_xt.h \
						ha_pbxt.h heap_xt.h index_xt.h linklist_xt.h \
						memory_xt.h myxt_xt.h pthread_xt.h restart_xt.h \
						pbms_enabled.h sortedlist_xt.h strutil_xt.h \
						tabcache_xt.h table_xt.h trace_xt.h thread_xt.h \
						util_xt.h xaction_xt.h xactlog_xt.h lock_xt.h \
						systab_xt.h ha_xtsys.h discover_xt.h \
						pbms.h xt_config.h xt_defs.h xt_errno.h locklist_xt.h
EXTRA_LTLIBRARIES =	libpbxt.la

libpbxt_la_SOURCES =	bsearch_xt.cc cache_xt.cc ccutils_xt.cc database_xt.cc \
						datadic_xt.cc datalog_xt.cc filesys_xt.cc hashtab_xt.cc \
						ha_pbxt.cc heap_xt.cc index_xt.cc linklist_xt.cc \
						memory_xt.cc myxt_xt.cc pthread_xt.cc restart_xt.cc \
						pbms_enabled.cc sortedlist_xt.cc strutil_xt.cc \
						tabcache_xt.cc table_xt.cc trace_xt.cc thread_xt.cc \
						systab_xt.cc ha_xtsys.cc discover_xt.cc \
						util_xt.cc xaction_xt.cc xactlog_xt.cc lock_xt.cc locklist_xt.cc

libpbxt_la_LDFLAGS =	-module

# These are the warning Drizzle uses:
# DRIZZLE_WARNINGS =		-W -Wall -Wextra -pedantic -Wundef -Wredundant-decls -Wno-strict-aliasing -Wno-long-long -Wno-unused-parameter

libpbxt_la_CXXFLAGS =	$(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libpbxt_la_CFLAGS =		$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN -std=c99

EXTRA_LIBRARIES =		libpbxt.a
noinst_LIBRARIES = 		libpbxt.a
libpbxt_a_SOURCES =		$(libpbxt_la_SOURCES)
libpbxt_a_CXXFLAGS =	$(AM_CXXFLAGS)
libpbxt_a_CFLAGS =		$(AM_CFLAGS) -std=c99

EXTRA_DIST =			CMakeLists.txt
