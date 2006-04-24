#Makefile.am example for a plugin

#MYSQL_MODULE(ftexample,         [Simple Parser],
#        [Simple full-text parser plugin])
#MYSQL_MODULE_DIRECTORY(ftexample, [plugin/fulltext])
#MYSQL_MODULE_STATIC(ftexample,  [libftexample.a])
#MYSQL_MODULE_DYNAMIC(ftexample, [ft_example.la])


#called from the top level Makefile

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
INCLUDES =              -I$(top_srcdir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir)
WRAPLIBS=

LDADD =

DEFS =                  @DEFS@

noinst_HEADERS =	

EXTRA_LTLIBRARIES =	ft_example.la
pkglib_LTLIBRARIES =	@plugin_ftexample_shared_target@
ft_example_la_LDFLAGS =	-module -rpath $(MYSQLLIBdir)
ft_example_la_CXXFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ft_example_la_CFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ft_example_la_SOURCES =	plugin_example.c


EXTRA_LIBRARIES =	libftexample.a
noinst_LIBRARIES =	@plugin_ftexample_static_target@
libftexample_a_CXXFLAGS=$(AM_CFLAGS)
libftexample_a_CFLAGS =	$(AM_CFLAGS)
libftexample_a_SOURCES=	plugin_example.c


# Don't update the files from bitkeeper
%::SCCS/s.%
