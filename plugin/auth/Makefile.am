pkgplugindir=$(pkglibdir)/plugin

AM_LDFLAGS=-module -rpath $(pkgplugindir)
AM_CPPFLAGS=-DMYSQL_DYNAMIC_PLUGIN -Wno-pointer-sign -I$(top_srcdir)/include

pkgplugin_LTLIBRARIES=  auth.la auth_test_plugin.la qa_auth_interface.la qa_auth_server.la qa_auth_client.la
auth_la_SOURCES= dialog.c
auth_test_plugin_la_SOURCES= test_plugin.c

qa_auth_interface_la_SOURCES= qa_auth_interface.c
qa_auth_server_la_SOURCES= qa_auth_server.c
qa_auth_client_la_SOURCES= qa_auth_client.c

if HAVE_PEERCRED
pkgplugin_LTLIBRARIES+= auth_socket.la
auth_socket_la_SOURCES= auth_socket.c
endif

EXTRA_DIST= plug.in

