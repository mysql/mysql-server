# Used to build Makefile.in

EXTRA_DIST = ha_tokudb.h CMakeFiles.txt plug.in tokudb_probes.d

# this is for regular mysql builds
# INCLUDES =   $(MYSQL_INC)

# this is for debug mysql builds
INCLUDES = -DDBUG_ON -DSAFE_MUTEX -DEXTRA_DEBUG -DSAFEMALLOC -DPEDANTIC_SAFEMALLOC -DSAFE_MUTEX $(MYSQL_INC)

LIBS=-ldb

DTRACE =                @DTRACE@
DTRACEFLAGS =           @DTRACEFLAGS@
DTRACEFILES =           .libs/libtokudb_engine_la-ha_tokudb.o

noinst_HEADERS = ha_tokudb.h \
		 tokudb_probes.h

lib_LTLIBRARIES = libtokudb_engine.la
libtokudb_engine_la_SOURCES = ha_tokudb.cc
libtokudb_engine_la_LIBADD =

if HAVE_DTRACE
  libtokudb_engine_la_LIBADD += tokudb_probes.o
endif

libtokudb_engine_la_LDFLAGS =	-module
libtokudb_engine_la_CFLAGS = $(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libtokudb_engine_la_CXXFLAGS = $(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN

tokudb_probes.h: tokudb_probes.d
	$(DTRACE) $(DTRACEFLAGS) -h -s tokudb_probes.d
	mv tokudb_probes.h tokudb_probes.h.bak
	sed "s/#include <unistd.h>//g" tokudb_probes.h.bak > tokudb_probes.h
	rm tokudb_probes.h.bak

tokudb_probes.o:
	$(DTRACE) $(DTRACEFLAGS) -G -s tokudb_probes.d $(DTRACEFILES)

# End
