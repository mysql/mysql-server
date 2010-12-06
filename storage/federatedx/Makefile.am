# Used to build Makefile.in

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =		$(pkglibdir)/plugin
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir)
WRAPLIBS=

LDADD =

DEFS =                  @DEFS@

noinst_HEADERS =	ha_federatedx.h federatedx_probes.h

EXTRA_LTLIBRARIES =	libfederatedx.la libfederatedx_common.la libfederatedx_embedded.la ha_federatedx.la
pkgplugin_LTLIBRARIES =	@plugin_federatedx_shared_target@
ha_federatedx_la_LDFLAGS =	-module -rpath $(pkgplugindir) \
				-L$(top_builddir)/libservices -lmysqlservices
ha_federatedx_la_CXXFLAGS=	-shared $(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_federatedx_la_CFLAGS =	-shared $(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN


noinst_LTLIBRARIES =	@plugin_federatedx_static_target@ @plugin_federatedx_embedded_static_target@
libfederatedx_common_la_CXXFLAGS =	$(AM_CXXFLAGS)
libfederatedx_common_la_CFLAGS =	$(AM_CFLAGS)
libfederatedx_common_la_SOURCES=	federatedx_txn.cc \
			        federatedx_io.cc federatedx_io_null.cc \
			        federatedx_io_mysql.cc

libfederatedx_la_CXXFLAGS =	$(AM_CFLAGS)
libfederatedx_la_CFLAGS =	$(AM_CFLAGS)
libfederatedx_la_LIBADD =	libfederatedx_common.la
libfederatedx_la_SOURCES=	ha_federatedx.cc

libfederatedx_embedded_la_CXXFLAGS =	$(AM_CFLAGS) @plugin_embedded_defs@
libfederatedx_embedded_la_CFLAGS =	$(AM_CFLAGS) @plugin_embedded_defs@
libfederatedx_embedded_la_LDFLAGS =	$(AM_LDFLAGS)
libfederatedx_embedded_la_LIBADD =	libfederatedx_common.la
libfederatedx_embedded_la_SOURCES=	ha_federatedx.cc

EXTRA_DIST =		CMakeLists.txt plug.in ha_federatedx.h \
			federatedx_probes.h

ha_federatedx_la_SOURCES = ha_federatedx.cc federatedx_txn.cc \
			   federatedx_io.cc federatedx_io_null.cc \
			   federatedx_io_mysql.cc $(top_srcdir)/mysys/string.c
ha_federatedx_la_LIBADD =

#DTRACE =                @DTRACE@
#DTRACEFLAGS =           @DTRACEFLAGS@
#DTRACEFILES =           .libs/libfederatedx_engine_la-ha_federatedx.o

# #if HAVE_DTRACE
# #  libfederatedx_engine_la_LIBADD += federatedx_probes.o
# #endif

# federatedx_probes.h: federatedx_probes.d
#	$(DTRACE) $(DTRACEFLAGS) -h -s federatedx_probes.d
#	mv federatedx_probes.h federatedx_probes.h.bak
#	sed "s/#include <unistd.h>//g" federatedx_probes.h.bak > federatedx_probes.h
#	rm federatedx_probes.h.bak

#federatedx_probes.o:
#	$(DTRACE) $(DTRACEFLAGS) -G -s federatedx_probes.d $(DTRACEFILES)

# End
