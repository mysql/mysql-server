#called from the top level Makefile

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir =          $(prefix)
MYSQLLIBdir =           $(pkglibdir)
pkgplugindir =          $(pkglibdir)/plugin
INCLUDES =              -I$(top_builddir)/include \
			-I$(top_srcdir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir) \
			-I$(TOKUFRACTALTREE)/include

LDADD =

DEFS =	@DEFS@

noinst_HEADERS	  =	

EXTRA_LTLIBRARIES =	ha_tokudb.la
pkgplugin_LTLIBRARIES =	@plugin_tokudb_shared_target@
ha_tokudb_la_LIBADD =   -L$(TOKUFRACTALTREE)/lib -l$(TOKUFRACTALTREE_LIBNAME) -l$(TOKUPORTABILITY_LIBNAME) -lz
ha_tokudb_la_LDFLAGS =	-module -rpath $(pkgplugindir)
ha_tokudb_la_CXXFLAGS =	$(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN -DTOKUDB_VERSION=\"$(TOKUDB_VERSION)\" -Wall
ha_tokudb_la_SOURCES =	ha_tokudb.cc

EXTRA_LIBRARIES =	libtokudb.a
noinst_LIBRARIES =	@plugin_tokudb_static_target@
libtokudb_a_CXXFLAGS =	$(AM_CXXFLAGS) -Wall
libtokudb_a_SOURCES =	ha_tokudb.cc

EXTRA_DIST =		CMakeLists.txt plug.in
# Don't update the files from bitkeeper
%::SCCS/s.%
