#called from the top level Makefile

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
TOKUDBdir=              $(top_builddir)/../tokudb/release
INCLUDES =              -I$(top_builddir)/include \
			-I$(top_srcdir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir) \
			-I$(TOKUDBdir)/include
LDADD =

DEFS =	@DEFS@
noinst_HEADERS	  =	

EXTRA_LTLIBRARIES =	ha_tokudb.la
pkglib_LTLIBRARIES =	@plugin_tokudb_shared_target@
ha_tokudb_la_LIBADD =   -L$(TOKUDBdir)/lib -ltokudb -ltokuportability
ha_tokudb_la_LDFLAGS =	-module -rpath $(MYSQLLIBdir)
ha_tokudb_la_CXXFLAGS =	$(AM_CFLAGS) -DMYSQL_PLUGIN
ha_tokudb_la_SOURCES =	hatoku_hton.cc hatoku_cmp.cc ha_tokudb.cc

#EXTRA_LIBRARIES =	libtokudb.a
#noinst_LIBRARIES =	@plugin_tokudb_static_target@
#libtokudb_a_CXXFLAGS =	$(AM_CFLAGS)
#libtokudb_a_SOURCES =	ha_tokudb.cc

EXTRA_DIST =		CMakeLists.txt plug.in
# Don't update the files from bitkeeper
%::SCCS/s.%
