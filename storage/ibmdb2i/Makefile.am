# 
#  Copyright (c) 2007, 2008, IBM Corporation.
#  All rights reserved.
#
#

#called from the top level Makefile

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =		$(pkglibdir)/plugin
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir) \
                        -I$ /afs/rchland.ibm.com/lande/shadow/dev2000/osxpf/v5r4m0f.xpf/cur/cmvc/base.pgm/my.xpf/apis \
                        -I$ /afs/rchland.ibm.com/lande/shadow/dev2000/osxpf/v5r4m0.xpf/bld/cmvc/base.pgm/lg.xpf \
                        -I$ /afs/rchland.ibm.com/lande/shadow/dev2000/osxpf/v5r4m0.xpf/bld/cmvc/base.pgm/tq.xpf
WRAPLIBS=

LDADD =			

DEFS =                  @DEFS@

noinst_HEADERS =	ha_ibmdb2i.h db2i_collationSupport.h db2i_file.h \
			db2i_ioBuffers.h db2i_blobCollection.h \
			db2i_global.h db2i_misc.h db2i_charsetSupport.h db2i_errors.h \
			db2i_iconv.h db2i_myconv.h db2i_safeString.h db2i_sqlStatementStream.h \
			db2i_ileBridge.h db2i_validatedPointer.h

EXTRA_LTLIBRARIES =	ha_ibmdb2i.la
pkgplugin_LTLIBRARIES =	@plugin_ibmdb2i_shared_target@
ha_ibmdb2i_la_LIBADD =  -liconv
ha_ibmdb2i_la_LDFLAGS =	-module -rpath $(MYSQLLIBdir)
ha_ibmdb2i_la_CXXFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_ibmdb2i_la_CFLAGS =	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_ibmdb2i_la_SOURCES =	ha_ibmdb2i.cc db2i_ileBridge.cc db2i_conversion.cc \
			db2i_blobCollection.cc db2i_file.cc db2i_charsetSupport.cc \
                        db2i_collationSupport.cc db2i_errors.cc db2i_constraints.cc \
			db2i_rir.cc db2i_sqlStatementStream.cc db2i_ioBuffers.cc \
                        db2i_myconv.cc

EXTRA_LIBRARIES =	libibmdb2i.a
noinst_LIBRARIES =	@plugin_ibmdb2i_static_target@
libibmdb2i_a_CXXFLAGS =	$(AM_CFLAGS)
libibmdb2i_a_CFLAGS =	$(AM_CFLAGS)
libibmdb2i_a_SOURCES=	$(ha_ibmdb2i_la_SOURCES)


EXTRA_DIST =		CMakeLists.txt plug.in
# Don't update the files from bitkeeper
%::SCCS/s.%
