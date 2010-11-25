# Used to build Makefile.in

INCLUDES =	-I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/storage/innobase/include \
			-I$(top_srcdir)/sql \
			-I$(srcdir) \
			-I$(srcdir)/../src

bin_PROGRAMS =		xtstat

xtstat_SOURCES =	xtstat_xt.cc ../src/strutil_xt.cc

xtstat_LDADD =		$(top_builddir)/libmysql/libmysqlclient.la
