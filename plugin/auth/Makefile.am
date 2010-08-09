pkgplugindir=$(pkglibdir)/plugin

AM_LDFLAGS=-module -rpath $(pkgplugindir)
AM_CPPFLAGS=-DMYSQL_DYNAMIC_PLUGIN -Wno-pointer-sign -I$(top_srcdir)/include

pkgplugin_LTLIBRARIES=  auth.la auth_test_plugin.la
auth_la_SOURCES= dialog.c
auth_test_plugin_la_SOURCES= test_plugin.c

if HAVE_PEERCRED
pkgplugin_LTLIBRARIES+= auth_socket.la
auth_socket_la_SOURCES= auth_socket.c
endif

EXTRA_DIST= plug.in

