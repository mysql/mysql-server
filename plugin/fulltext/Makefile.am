#Makefile.am example for a plugin

pkglibdir=$(libdir)/mysql
INCLUDES= -I$(top_builddir)/include -I$(top_srcdir)/include
noinst_LTLIBRARIES= mypluglib.la
#pkglib_LTLIBRARIES= mypluglib.la
mypluglib_la_SOURCES= plugin_example.c
mypluglib_la_LDFLAGS= -module -rpath $(pkglibdir)

