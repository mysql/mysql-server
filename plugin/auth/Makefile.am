pkgplugindir=$(pkglibdir)/plugin

AM_LDFLAGS=-module -rpath $(pkgplugindir)
AM_CPPFLAGS=-DMYSQL_DYNAMIC_PLUGIN -I$(top_srcdir)/include

pkgplugin_LTLIBRARIES=  dialog.la
dialog_la_SOURCES= dialog.c

if HAVE_PEERCRED
pkgplugin_LTLIBRARIES+= auth_socket.la
auth_socket_la_SOURCES= auth_socket.c
endif

EXTRA_DIST= plug.in

