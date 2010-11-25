# Copyright (C) 2007-2009 Arjen G Lentz & Antony T Curtis for Open Query
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

# ======================================================================
# Open Query Graph Computation Engine, based on a concept by Arjen Lentz
# Mk.II implementation by Antony Curtis & Arjen Lentz
# For more information, documentation, support, enhancement engineering,
# and non-GPL licensing, see http://openquery.com/graph
# or contact graph@openquery.com
# For packaged binaries, see http://ourdelta.org
# ======================================================================

mysqlplugindir=		$(pkglibdir)/plugin

BOOST_CXXFLAGS =	-frtti -fexceptions -fimplicit-templates
#BOOST_CXXFLAGS+=	-g
#original flags before 2009-11-10
#BOOST_CXXFLAGS+=	-O3 -fomit-frame-pointer -fstrict-aliasing
#BOOST_CXXFLAGS+=	-momit-leaf-frame-pointer -falign-loops
#modified flags:
# - remove omit-frame-pointer, x86 specific (fails on PPC) + hinders debugging
#   Option details from gcc man:
#   Don't keep the frame pointer in a register for functions that don't need one.
#   This avoids the instructions to save, set up and restore frame pointers;
#   it also makes an extra register available in many functions.
#   It also makes debugging impossible on some machines.
#   (automatically gets enabled anyway by -O* on some architectures)
BOOST_CXXFLAGS+=	-O3 -fstrict-aliasing
BOOST_CXXFLAGS+=	-falign-loops
if HAVE_FVISIBILITY_INLINES_HIDDEN
BOOST_CXXFLAGS+=	-fvisibility-inlines-hidden
endif
BOOST_CXXFLAGS+=	-funroll-loops -fno-trapping-math

EXTRA_DIST =	ha_oqgraph.h ha_oqgraph.cc graphcore.cc \
		graphcore-graph.h graphcore-types.h graphcore.h \
		CMakeLists.txt plug.in oqgraph_probes.d

# DTRACE =                @DTRACE@
# DTRACEFLAGS =           @DTRACEFLAGS@
# DTRACEFILES =           .libs/libha_oqgraph_la-ha_oqgraph.o

ORIG_CXXFLAGS = @CXXFLAGS@
CXXFLAGS=
noinst_HEADERS = ha_oqgraph.h \
		 graphcore-graph.h graphcore-types.h graphcore.h
#		 oqgraph_probes.h

noinst_LTLIBRARIES = libgraphcore.la
libgraphcore_la_SOURCES = graphcore.cc
libgraphcore_la_CXXFLAGS = $(ORIG_CXXFLAGS) $(BOOST_CXXFLAGS)

if BUILD_OQGRAPH_FOR_MYSQL

if BUILD_OQGRAPH_STANDALONE
INCLUDES = -DDBUG_ON -DSAFE_MUTEX -DUNIV_MUST_NOT_INLINE -DEXTRA_DEBUG -DFORCE_INIT_OF_VARS -DSAFEMALLOC -DPEDANTIC_SAFEMALLOC -DSAFE_MUTEX -DHAVE_OQGRAPH $(MYSQL_INC) 
else
INCLUDES = -I$(top_srcdir)/include -I$(top_builddir)/include -I$(top_srcdir)/regex -I$(top_srcdir)/sql -I$(srcdir) -DHAVE_OQGRAPH 
endif !BUILD_OQGRAPH_STANDALONE

EXTRA_LTLIBRARIES = ha_oqgraph.la
mysqlplugin_LTLIBRARIES = @plugin_oqgraph_shared_target@
ha_oqgraph_la_SOURCES = ha_oqgraph.cc
ha_oqgraph_la_LIBADD = libgraphcore.la

# if HAVE_DTRACE
#   ha_oqgraph_la_LIBADD += oqgraph_probes.o
# endif

ha_oqgraph_la_LDFLAGS =	-shared -module -rpath $(mysqlplugindir)
ha_oqgraph_la_CFLAGS = $(ORIG_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_oqgraph_la_CXXFLAGS = $(ORIG_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN

# oqgraph_probes.h: oqgraph_probes.d
# 	$(DTRACE) $(DTRACEFLAGS) -h -s oqgraph_probes.d
# 	mv oqgraph_probes.h oqgraph_probes.h.bak
# 	sed "s/#include <unistd.h>//g" oqgraph_probes.h.bak > oqgraph_probes.h
# 	rm oqgraph_probes.h.bak

# oqgraph_probes.o:
# 	$(DTRACE) $(DTRACEFLAGS) -G -s oqgraph_probes.d $(DTRACEFILES)

endif BUILD_OQGRAPH_FOR_MYSQL

# End
