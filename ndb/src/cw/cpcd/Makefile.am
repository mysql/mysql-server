
ndbbin_PROGRAMS = ndb_cpcd

ndb_cpcd_SOURCES = main.cpp CPCD.cpp Process.cpp APIService.cpp Monitor.cpp common.cpp

LDADD_LOC = \
            $(top_builddir)/ndb/src/libndbclient.la      \
            $(top_builddir)/dbug/libdbug.a \
            $(top_builddir)/mysys/libmysys.a \
            $(top_builddir)/strings/libmystrings.a @NDB_SCI_LIBS@

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

ndb_cpcd_LDFLAGS = @ndb_bin_am_ldflags@

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp:
