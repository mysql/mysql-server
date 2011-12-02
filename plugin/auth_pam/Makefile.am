EXTRA_LTLIBRARIES      = auth_pam.la libauth_pam.la

pkgplugindir=$(pkglibdir)/plugin
AM_CPPFLAGS = -I$(top_srcdir)/include

pkgplugin_LTLIBRARIES  = @plugin_auth_pam_shared_target@
auth_pam_la_LDFLAGS    = -module -rpath $(pkgplugindir) -L$(top_builddir)/libservices -lmysqlservices -lpam
auth_pam_la_CFLAGS     = -shared -DMYSQL_DYNAMIC_PLUGIN
auth_pam_la_SOURCES    = auth_pam.c

noinst_LTLIBRARIES     = @plugin_auth_pam_static_target@
libauth_pam_la_LDFLAGS = -lpam
libauth_pam_la_SOURCES = auth_pam.c

EXTRA_DIST = plug.in

