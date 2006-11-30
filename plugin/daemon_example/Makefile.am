#Makefile.am example for a daemon
MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
                        -I$(srcdir) 

EXTRA_LTLIBRARIES =	libdaemon_example.la
pkglib_LTLIBRARIES =	@plugin_daemon_example_shared_target@
libdaemon_example_la_LDFLAGS =	-module -rpath $(MYSQLLIBdir)
libdaemon_example_la_CXXFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libdaemon_example_la_CFLAGS =	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libdaemon_example_la_SOURCES =	daemon_example.c


EXTRA_LIBRARIES =	libdaemon_example.a
noinst_LIBRARIES =	@plugin_daemon_example_static_target@
libdaemon_example_a_CXXFLAGS =	$(AM_CFLAGS)
libdaemon_example_a_CFLAGS =	$(AM_CFLAGS)
libdaemon_example_a_SOURCES=	daemon_example.c
